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
                     std::optional<ssl_config> ssl_cfg,
                     std::optional<cors_config> cors_cfg)
    : _address(address)
    , _port(port)
    , _storage_path(storage_path)
    , _keys_file(keys_file)
    , _users_file(users_file)
    , _acceptor(_io_context)
    , _ssl_config(std::move(ssl_cfg))
    , _cors_config(std::move(cors_cfg))
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
    
    // Initialize rate limiter with default configuration
    _rate_limiter = std::make_unique<rate_limiter>();
    LOG(INFO) << "Rate limiting enabled with default configuration";
    
    // Log CORS configuration
    if (_cors_config.has_value()) {
        LOG(INFO) << "CORS enabled with configuration:";
        LOG(INFO) << "  Allowed origins: " << _cors_config->get_allowed_origins_header();
        LOG(INFO) << "  Allowed methods: " << _cors_config->get_allowed_methods_header();
        LOG(INFO) << "  Allowed headers: " << _cors_config->get_allowed_headers_header();
        LOG(INFO) << "  Exposed headers: " << _cors_config->get_exposed_headers_header();
        LOG(INFO) << "  Allow credentials: " << (_cors_config->allow_credentials ? "true" : "false");
        LOG(INFO) << "  Max age: " << _cors_config->max_age << " seconds";
    } else {
        LOG(INFO) << "CORS disabled (using default permissive configuration)";
    }
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
                // Get client IP address for rate limiting
                std::string client_ip = "unknown";
                try {
                    auto remote_endpoint = socket.remote_endpoint();
                    client_ip = remote_endpoint.address().to_string();
                } catch (const std::exception& e) {
                    LOG(WARNING) << "Failed to get client IP: " << e.what();
                }
                
                VLOG(1) << "New connection accepted from " << client_ip;
                
                // Check connection rate limiting
                if (_rate_limiter && !_rate_limiter->allow_connection(client_ip)) {
                    LOG(WARNING) << "Connection rate limit exceeded for IP: " << client_ip;
                    socket.close();
                    
                    // Continue accepting new connections
                    if (_running) {
                        do_accept();
                    }
                    return;
                }
                
                // Record the connection
                if (_rate_limiter) {
                    _rate_limiter->record_connection(client_ip);
                }
                
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
                            [this, ssl_socket, client_ip](const beast::error_code& ec_handshake) {
                                if (!ec_handshake) {
                                    // Запускаем обработку зашифрованной сессии
                                    std::thread([this, ssl_socket, client_ip]() mutable {
                                        handle_ssl_session(std::move(*ssl_socket), client_ip);
                                    }).detach();
                                } else {
                                    LOG(ERROR) << "SSL handshake failed: " << ec_handshake.message();
                                    // Record disconnection on handshake failure
                                    if (_rate_limiter) {
                                        _rate_limiter->record_disconnection(client_ip);
                                    }
                                }
                            }
                        );
                    }
                } else {
                    // Запускаем обработку обычной сессии
                    std::thread([this, socket = std::move(socket), client_ip]() mutable {
                        handle_session(std::move(socket), client_ip);
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

void s3_server::handle_session(tcp::socket socket, const std::string& client_ip)
{
    auto start_time = std::chrono::steady_clock::now();
    
    // Record disconnection when session ends
    auto cleanup = [this, client_ip]() {
        if (_rate_limiter && !client_ip.empty() && client_ip != "unknown") {
            _rate_limiter->record_disconnection(client_ip);
        }
    };
    
    try {
        beast::error_code ec;
        
        // Use provided client_ip or get from socket
        std::string client_info = client_ip;
        if (client_info.empty() || client_info == "unknown") {
            auto remote_endpoint = socket.remote_endpoint(ec);
            if (!ec) {
                client_info = remote_endpoint.address().to_string() +
                             ":" + std::to_string(remote_endpoint.port());
            }
        }
        
        VLOG(2) << "Handling session from " << client_info;
        
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
            cleanup();
            return;
        }
        
        // Получаем запрос из парсера
        http::request<http::string_body> req = parser.release();
        
        // Extract IP address from client_info (format: "ip:port")
        std::string client_ip = client_info;
        size_t colon_pos = client_info.find(':');
        if (colon_pos != std::string::npos) {
            client_ip = client_info.substr(0, colon_pos);
        }
        
        // Check rate limiting before processing request
        if (_rate_limiter && !client_ip.empty() && client_ip != "unknown") {
            if (!_rate_limiter->allow_request(client_ip)) {
                LOG(WARNING) << "Rate limit exceeded for IP: " << client_ip;
                
                // Send 429 Too Many Requests response
                http::response<http::string_body> res{http::status::too_many_requests, 11};
                res.set(http::field::server, "S3-Server");
                res.set(http::field::content_type, "application/json");
                res.body() = R"({"error": "rate_limit_exceeded", "message": "Too many requests, please try again later."})";
                res.prepare_payload();
                
                http::write(socket, res, ec);
                socket.shutdown(tcp::socket::shutdown_send, ec);
                socket.close(ec);

                cleanup();
                return;
            }
            
            // Check request size limit
            size_t request_size = req.body().size();
            if (!_rate_limiter->check_request_size(client_ip, request_size)) {
                LOG(WARNING) << "Request size limit exceeded for IP: " << client_ip;
                
                // Send 413 Payload Too Large response
                http::response<http::string_body> res{http::status::payload_too_large, 11};
                res.set(http::field::server, "S3-Server");
                res.set(http::field::content_type, "application/json");
                res.body() = R"({"error": "request_too_large", "message": "Request size exceeds allowed limit."})";
                res.prepare_payload();
                
                http::write(socket, res, ec);
                socket.shutdown(tcp::socket::shutdown_send, ec);
                socket.close(ec);

                cleanup();
                return;
            }
            
            // Record the request (will be recorded again after successful processing)
            _rate_limiter->record_request(client_ip, request_size);
        }
        
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
        handler.set_cors_config(_cors_config);
        
        // Обрабатываем запрос
        handler.handle_request(
            std::move(req),
            [this, &socket, start_time, cleanup](http::response<http::string_body>&& res) {
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

                cleanup();
            }
        );
    } catch (const std::exception& e) {
        LOG(ERROR) << "Session error: " << e.what();
    }
}

void s3_server::handle_ssl_session(ssl::stream<tcp::socket> socket, const std::string& client_ip)
{
    auto start_time = std::chrono::steady_clock::now();
    
    // Record disconnection when session ends
    auto cleanup = [this, client_ip]() {
        if (_rate_limiter && !client_ip.empty() && client_ip != "unknown") {
            _rate_limiter->record_disconnection(client_ip);
        }
    };

    beast::error_code ec;

    // Use provided client_ip or get from socket
    std::string client_info = client_ip;
    if (client_info.empty() || client_info == "unknown") {
        auto remote_endpoint = socket.next_layer().remote_endpoint(ec);
        if (!ec) {
            client_info = remote_endpoint.address().to_string() + ":" + std::to_string(remote_endpoint.port());
        }
    }

    VLOG(2) << "Handling HTTPS session from " << client_info;

    std::string clean_ip = client_info;
    size_t colon_pos = client_info.find(':');
    if (colon_pos != std::string::npos) {
        clean_ip = client_info.substr(0, colon_pos);
    }

    // === RATE LIMITING для SSL ===
    if (_rate_limiter && !clean_ip.empty() && clean_ip != "unknown") {
        if (!_rate_limiter->allow_connection(clean_ip)) {
            LOG(WARNING) << "SSL connection rate limit exceeded for IP: " << clean_ip;
            socket.next_layer().close(ec);
            return;
        }
        _rate_limiter->record_connection(clean_ip);
    }

    try {

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
            cleanup();
            return;
        }
        
        // Получаем запрос из парсера
        http::request<http::string_body> req = parser.release();

        // Rate limiting для запроса (проверка лимитов и размера)
        if (_rate_limiter && !clean_ip.empty() && clean_ip != "unknown") {
            if (!_rate_limiter->allow_request(clean_ip)) {
                LOG(WARNING) << "SSL rate limit exceeded for IP: " << clean_ip;
                http::response<http::string_body> res{http::status::too_many_requests, 11};
                res.set(http::field::content_type, "application/json");
                res.body() = R"({"error": "rate_limit_exceeded", "message": "Too many requests"})";
                res.prepare_payload();
                http::write(socket, res, ec);
                socket.shutdown(ec);
                socket.next_layer().close(ec);
                cleanup();
                return;
            }
            
            size_t request_size = req.body().size();
            if (!_rate_limiter->check_request_size(clean_ip, request_size)) {
                LOG(WARNING) << "SSL request size limit exceeded for IP: " << clean_ip;
                http::response<http::string_body> res{http::status::payload_too_large, 11};
                res.set(http::field::content_type, "application/json");
                res.body() = R"({"error": "request_too_large", "message": "Request size exceeds limit"})";
                res.prepare_payload();
                http::write(socket, res, ec);
                socket.shutdown(ec);
                socket.next_layer().close(ec);
                cleanup();
                return;
            }
            
            _rate_limiter->record_request(clean_ip, request_size);
        }

        // ЯВНОЕ ПРЕОБРАЗОВАНИЕ string_view В string
        logging::log_request(
            std::string(req.method_string()),
            std::string(req.target()),
            client_info
        );
        
        // Создаем менеджер файлов и обработчик запросов
        file_manager fm(_storage_path);
        // ПЕРЕДАЁМ аутентификатор, авторизатор и CORS
        request_handler handler(fm, _authenticator.get(), _authorizer.get());
        handler.set_auth_enabled(_auth_enabled);
        handler.set_authorization_enabled(_authorization_enabled);
        handler.set_cors_config(_cors_config);
        
        // Обрабатываем запрос
        handler.handle_request(
            std::move(req),
            [&socket, start_time, cleanup](http::response<http::string_body>&& res) {
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

                cleanup();
            }
        );
    } catch (const std::exception& e) {
        LOG(ERROR) << "SSL session error: " << e.what();
        cleanup();
    }
}