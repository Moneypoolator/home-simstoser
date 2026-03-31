#include "server.hpp"
#include "request_handler.hpp"
#include "logging.hpp"
#include <iostream>
#include <memory>
#include <chrono>
#include <glog/logging.h>

namespace ssl = asio::ssl;

s3_server::s3_server(const std::string& address, 
                     unsigned short port, 
                     const std::string& storage_path,
                     const std::string& keys_file,
                     const std::string& users_file,
                     std::optional<ssl_config> ssl_cfg)
    : _address(address)
    , _port(port)
    , _storage_path(storage_path)
    , _keys_file(keys_file)
    , _users_file(users_file)
    , _acceptor(_io_context)
    , _ssl_config(std::move(ssl_cfg))
{
    _ssl_enabled = _ssl_config.has_value();
    
    // Инициализируем аутентификатор, если указан файл ключей
    if (!_keys_file.empty()) {
        _authenticator = std::make_unique<authenticator>();
        
        if (_authenticator->load_keys(_keys_file)) {
            _auth_enabled = true;
            LOG(INFO) << "Authentication enabled with keys file: " << _keys_file;
        } else {
            LOG(WARNING) << "Failed to load access keys, authentication disabled";
        }
    }
    
    // Инициализируем авторизатор, если указан файл пользователей
    if (!_users_file.empty()) {
        _authorizer = std::make_unique<authorizer>();
        _authorization_enabled = true;
        LOG(INFO) << "Authorization enabled with users file: " << _users_file;
        
        // TODO: Загрузка пользователей из файла
    }
    
    if (_ssl_enabled) {
        LOG(INFO) << "S3 server created with SSL: " << address << ":" << port;
        LOG(INFO) << "Certificate: " << _ssl_config->cert_file;
    } else {
        LOG(INFO) << "S3 server created (HTTP only): " << address << ":" << port;
    }
    
    LOG(INFO) << "Storage path: " << storage_path;
}

s3_server::~s3_server()
{
    stop();
}

void s3_server::run(std::size_t threads)
{
    if (threads == 0) {
        threads = std::thread::hardware_concurrency();
    }
    
    try {
        // Открываем acceptor
        tcp::endpoint endpoint(asio::ip::make_address(_address), _port);
        _acceptor.open(endpoint.protocol());
        _acceptor.set_option(asio::socket_base::reuse_address(true));
        _acceptor.bind(endpoint);
        _acceptor.listen(asio::socket_base::max_listen_connections);
        
        LOG(INFO) << "Starting S3-compatible server on " 
                  << (_ssl_enabled ? "https" : "http") 
                  << "://" << _address << ":" << _port;
        LOG(INFO) << "Using " << threads << " threads";
        
        _running = true;
        
        // Запускаем первую асинхронную операцию принятия соединения
        do_accept();
        
        // Запускаем пул потоков
        _threads.reserve(threads);
        for (std::size_t i = 0; i < threads; ++i) {
            _threads.emplace_back([this] {
                try {
                    _io_context.run();
                } catch (const std::exception& e) {
                    LOG(ERROR) << "Thread exception: " << e.what();
                }
            });
        }
        
        LOG(INFO) << "Server started successfully";
    } catch (const std::exception& e) {
        LOG(FATAL) << "Failed to start server: " << e.what();
        throw;
    }
}

void s3_server::stop()
{
    LOG(INFO) << "Stopping server...";
    
    _running = false;
    _acceptor.close();
    _io_context.stop();
    
    for (auto& thread : _threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    _threads.clear();
    
    LOG(INFO) << "Server stopped";
}

void s3_server::do_accept()
{
    if (!_running) {
        return;
    }
    
    _acceptor.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                VLOG(1) << "New connection accepted";
                
                if (_ssl_enabled) {
                    // Создаем и настраиваем SSL контекст
                    auto ssl_ctx = setup_ssl_context();
                    if (ssl_ctx) {
                        // Создаем зашифрованное соединение
                        auto ssl_socket = std::make_shared<ssl::stream<tcp::socket>>(
                            std::move(socket), *ssl_ctx
                        );
                        
                        // Выполняем рукопожатие SSL
                        ssl_socket->async_handshake(
                            ssl::stream_base::server,
                            [this, ssl_socket](const beast::error_code& ec_handshake) {
                                if (!ec_handshake) {
                                    // Запускаем обработку зашифрованной сессии
                                    std::thread([this, ssl_socket]() mutable {
                                        handle_ssl_session(std::move(*ssl_socket));
                                    }).detach();
                                } else {
                                    LOG(ERROR) << "SSL handshake failed: " << ec_handshake.message();
                                }
                            }
                        );
                    }
                } else {
                    // Запускаем обработку обычной сессии
                    std::thread([this, socket = std::move(socket)]() mutable {
                        handle_session(std::move(socket));
                    }).detach();
                }
            } else {
                if (ec != asio::error::operation_aborted) {
                    LOG(WARNING) << "Accept error: " << ec.message();
                }
            }
            
            // Продолжаем принимать новые соединения
            if (_running) {
                do_accept();
            }
        }
    );
}

std::shared_ptr<ssl::context> s3_server::setup_ssl_context()
{
    try {
        auto ctx = std::make_shared<ssl::context>(ssl::context::tlsv12_server);
        
        // Загружаем сертификат сервера
        ctx->use_certificate_chain_file(_ssl_config->cert_file);
        
        // Загружаем приватный ключ
        ctx->use_private_key_file(_ssl_config->private_key, ssl::context::pem);
        
        // Устанавливаем опции для безопасности
        ctx->set_options(
            ssl::context::default_workarounds |
            ssl::context::no_sslv2 |
            ssl::context::no_sslv3 |
            ssl::context::no_tlsv1 |
            ssl::context::no_tlsv1_1 |
            ssl::context::single_dh_use
        );
        
        // Настройка Diffie-Hellman параметров (если указаны)
        if (_ssl_config->dh_file) {
            ctx->use_tmp_dh_file(*_ssl_config->dh_file);
        }
        
        // Настройка проверки клиента (если требуется)
        if (_ssl_config->verify_client) {
            ctx->set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
        } else {
            ctx->set_verify_mode(ssl::verify_none);
        }
        
        VLOG(1) << "SSL context configured successfully";
        return ctx;
        
    } catch (const std::exception& e) {
        LOG(ERROR) << "Failed to setup SSL context: " << e.what();
        return nullptr;
    }
}

void s3_server::handle_session(tcp::socket socket)
{
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        beast::error_code ec;
        
        // Получаем информацию о клиенте
        auto remote_endpoint = socket.remote_endpoint(ec);
        std::string client_info;
        if (!ec) {
            client_info = remote_endpoint.address().to_string() +
                         ":" + std::to_string(remote_endpoint.port());
            VLOG(2) << "Handling session from " << client_info;
        }
        
        // Создаем объекты для чтения/записи
        beast::flat_buffer buffer;
        
        // Создаем парсер с увеличенным лимитом размера тела
        http::request_parser<http::string_body> parser;
        // Устанавливаем лимит тела запроса (100 MB)
        parser.body_limit(100 * 1024 * 1024); // 100 MB
        
        // Читаем запрос
        http::read(socket, buffer, parser, ec);
        
        if (ec) {
            if (ec != beast::http::error::end_of_stream) {
                LOG(WARNING) << "Read error: " << ec.message();
            }
            return;
        }
        
        // Получаем запрос из парсера
        http::request<http::string_body> req = parser.release();
        
        logging::log_request(
            std::string(req.method_string()),
            std::string(req.target()),
            client_info
        );
        
        // Создаем менеджер файлов и обработчик запросов
        file_manager fm(_storage_path);
        request_handler handler(fm, _authenticator.get(), _authorizer.get());
        handler.set_auth_enabled(_auth_enabled);
        handler.set_authorization_enabled(_authorization_enabled);
        
        // Обрабатываем запрос
        handler.handle_request(
            std::move(req),
            [&socket, start_time](http::response<http::string_body>&& res) {
                beast::error_code ec;
                
                // Отправляем ответ
                http::write(socket, res, ec);
                
                if (ec) {
                    LOG(ERROR) << "Write error: " << ec.message();
                } else {
                    // Логируем время обработки
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start_time
                    );
                    VLOG(1) << "Request handled in " << duration.count() << "ms";
                    logging::log_response(static_cast<int>(res.result()));
                }
                
                // Закрываем соединение
                socket.shutdown(tcp::socket::shutdown_send, ec);
                socket.close(ec);
            }
        );
    } catch (const std::exception& e) {
        LOG(ERROR) << "Session error: " << e.what();
    }
}

void s3_server::handle_ssl_session(ssl::stream<tcp::socket> socket)
{
    auto start_time = std::chrono::steady_clock::now();
    
    try {
        beast::error_code ec;
        
        // Получаем информацию о клиенте
        auto remote_endpoint = socket.next_layer().remote_endpoint(ec);
        std::string client_info;
        if (!ec) {
            client_info = remote_endpoint.address().to_string() +
                         ":" + std::to_string(remote_endpoint.port());
            VLOG(2) << "Handling HTTPS session from " << client_info;
        }
        
        // Создаем объекты для чтения/записи
        beast::flat_buffer buffer;
        
        // Создаем парсер с увеличенным лимитом размера тела
        http::request_parser<http::string_body> parser;
        // Устанавливаем лимит тела запроса (100 MB)
        parser.body_limit(100 * 1024 * 1024); // 100 MB
        
        // Читаем запрос через зашифрованное соединение
        http::read(socket, buffer, parser, ec);
        
        if (ec) {
            if (ec != beast::http::error::end_of_stream) {
                LOG(WARNING) << "SSL read error: " << ec.message();
            }
            return;
        }
        
        // Получаем запрос из парсера
        http::request<http::string_body> req = parser.release();
        
        // ЯВНОЕ ПРЕОБРАЗОВАНИЕ string_view В string
        logging::log_request(
            std::string(req.method_string()),
            std::string(req.target()),
            client_info
        );
        
        // Создаем менеджер файлов и обработчик запросов
        file_manager fm(_storage_path);
        request_handler handler(fm);
        
        // Обрабатываем запрос
        handler.handle_request(
            std::move(req),
            [&socket, start_time](http::response<http::string_body>&& res) {
                beast::error_code ec;
                
                // Отправляем ответ через зашифрованное соединение
                http::write(socket, res, ec);
                if (ec) {
                    LOG(ERROR) << "SSL write error: " << ec.message();
                } else {
                    // Логируем время обработки
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start_time
                    );
                    VLOG(1) << "SSL request handled in " << duration.count() << "ms";
                    
                    // Логируем ответ
                    logging::log_response(static_cast<int>(res.result()));
                }
                
                // Закрываем зашифрованное соединение
                socket.shutdown(ec);
                socket.next_layer().shutdown(tcp::socket::shutdown_send, ec);
                socket.next_layer().close(ec);
            }
        );
    } catch (const std::exception& e) {
        LOG(ERROR) << "SSL session error: " << e.what();
    }
}