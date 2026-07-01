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
    const std::string& acls_file,
    std::optional<ssl_config> ssl_cfg,
    std::optional<cors_config> cors_cfg,
    upload_limits_config upload_limits,
    keep_alive_config keep_alive,
    rate_limiter_config rate_limiter,
    bool enable_unprotected)
    : _address(address)
    , _port(port)
    , _storage_path(storage_path)
    , _keys_file(keys_file)
    , _users_file(users_file)
    , _acls_file(acls_file)
    , _io_context()
    , _acceptor(_io_context)
    , _cleanup_timer(_io_context)
    , _ssl_config(std::move(ssl_cfg))
    , _cors_config(std::move(cors_cfg))
    , _unprotected(enable_unprotected)
    , _upload_limits(std::move(upload_limits))
    , _keep_alive_config(std::move(keep_alive))
    , _rate_limiter_config(std::move(rate_limiter))
{
    if (_unprotected) {
        LOG(WARNING) << "============================================";
        LOG(WARNING) << "  UNPROTECTED MODE ENABLED";
        LOG(WARNING) << "  Authentication, authorization, SSL, rate";
        LOG(WARNING) << "  limiting and CORS restrictions are OFF.";
        LOG(WARNING) << "  This mode is for DEBUG/TEST only!";
        LOG(WARNING) << "============================================";
        
        // Force-disable all security features
        _ssl_enabled = false;
        _auth_enabled = false;
        _authorization_enabled = false;
        _ssl_config = std::nullopt;
        _cors_config = std::nullopt;
        _authenticator = nullptr;
        _authorizer = nullptr;
        _rate_limiter = nullptr;
        
        LOG(INFO) << "S3 server created in UNPROTECTED mode: " << address << ":" << port;
        LOG(INFO) << "Storage path: " << storage_path;
        return;
    }
    
    _ssl_enabled = _ssl_config.has_value();
    
    // Инициализируем аутентификатор, если указан файл ключей
    if (!_keys_file.empty()) {
        _authenticator = std::make_shared<authenticator>();
        
        if (_authenticator->load_keys(_keys_file)) {
            _auth_enabled = true;
            LOG(INFO) << "Authentication enabled with keys file: " << _keys_file;
        } else {
            LOG(WARNING) << "Failed to load access keys, authentication disabled";
        }
    }
    
    // Инициализируем авторизатор, если указан файл пользователей или ACL
    if (!_users_file.empty() || !_acls_file.empty()) {
        _authorizer = std::make_shared<authorizer>();
        _authorization_enabled = true;

        if (!_users_file.empty()) {
            if (!_authorizer->load_users(_users_file)) {
                LOG(WARNING) << "Failed to load users from " << _users_file
                             << ", starting with empty user list";
            } else {
                LOG(INFO) << "Loaded users from " << _users_file;
            }
        }

        if (!_acls_file.empty()) {
            if (!_authorizer->load_acls(_acls_file)) {
                LOG(WARNING) << "Failed to load ACLs from " << _acls_file
                             << ", starting with empty ACL list";
            } else {
                LOG(INFO) << "Loaded ACLs from " << _acls_file;
            }
        }

        LOG(INFO) << "Authorization enabled";
    }
    
    if (_ssl_enabled) {
        LOG(INFO) << "S3 server created with SSL: " << address << ":" << port;
        LOG(INFO) << "Certificate: " << _ssl_config->cert_file;
    } else {
        LOG(INFO) << "S3 server created (HTTP only): " << address << ":" << port;
    }
    
    LOG(INFO) << "Storage path: " << storage_path;
    
    // Initialize rate limiter with configuration
    _rate_limiter = std::make_unique<::rate_limiter>(_rate_limiter_config);
    LOG(INFO) << "Rate limiting enabled with configuration";
    
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
    
    // Log keep-alive configuration
    LOG(INFO) << "Keep-alive configuration:";
    LOG(INFO) << "  Enabled: " << (_keep_alive_config.enabled ? "true" : "false");
    LOG(INFO) << "  Timeout: " << _keep_alive_config.timeout_seconds << " seconds";
    LOG(INFO) << "  Max requests per connection: " << _keep_alive_config.max_requests;
}

s3_server::~s3_server()
{
    if (_authorizer && !_users_file.empty()) {
        if (!_authorizer->save_users(_users_file)) {
            LOG(ERROR) << "Save user autorization file  error: " << _users_file;
        }
    }
    stop();
}

void s3_server::start_cleanup_timer()
{
    if (!_rate_limiter) {
        return;
    }
    
    _cleanup_timer.expires_after(CLEANUP_INTERVAL);
    _cleanup_timer.async_wait([this](const boost::system::error_code& ec) {
        if (ec) {
            if (ec != boost::asio::error::operation_aborted) {
                LOG(ERROR) << "Cleanup timer error: " << ec.message();
            }
            return;
        }
        
        // Perform cleanup
        VLOG(2) << "Running rate limiter cleanup";
        _rate_limiter->cleanup();
        
        // Schedule next cleanup
        start_cleanup_timer();
    });
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
        
        // Start periodic cleanup timer for rate limiter
        start_cleanup_timer();
        
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
    _cleanup_timer.cancel();
    
    // Wait for active sessions to finish (graceful shutdown)
    constexpr int shutdown_timeout_seconds = 30;
    LOG(INFO) << "Waiting for active connections to finish (max " << shutdown_timeout_seconds << " seconds)...";
    {
        std::unique_lock<std::mutex> lock(_shutdown_mutex);
        if (_shutdown_cv.wait_for(lock, std::chrono::seconds(shutdown_timeout_seconds),
            [this]() { return _active_sessions == 0; })) {
            LOG(INFO) << "All active connections closed gracefully";
        } else {
            LOG(WARNING) << "Timeout waiting for active connections, forcing shutdown";
        }
    }
    
    // Stop IO context and join worker threads
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
            
            // Загружаем CA сертификаты для проверки клиентских сертификатов
            bool ca_loaded = false;
            if (_ssl_config->ca_file) {
                ctx->load_verify_file(*_ssl_config->ca_file);
                ca_loaded = true;
                LOG(INFO) << "Loaded CA certificates from file: " << *_ssl_config->ca_file;
            }
            if (_ssl_config->ca_path) {
                ctx->add_verify_path(*_ssl_config->ca_path);
                ca_loaded = true;
                LOG(INFO) << "Added CA certificates path: " << *_ssl_config->ca_path;
            }
            if (!ca_loaded) {
                // Используем системные CA сертификаты по умолчанию
                ctx->set_default_verify_paths();
                LOG(INFO) << "Using system default CA certificates for client verification";
            }
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

void s3_server::handle_session(tcp::socket socket, const std::string& client_ip) {
    // Increment active sessions counter
    ++_active_sessions;
    
    std::string client_info = client_ip;
    beast::error_code ec;
    
    if (client_info.empty() || client_info == "unknown") {
        auto remote_endpoint = socket.remote_endpoint(ec);
        if (!ec) {
            client_info = remote_endpoint.address().to_string() +
                         ":" + std::to_string(remote_endpoint.port());
        }
    }
    
    std::string clean_ip = client_info;
    size_t colon_pos = client_info.find(':');
    if (colon_pos != std::string::npos) {
        clean_ip = client_info.substr(0, colon_pos);
    }
    
    // Decrement on exit
    auto decrement_session = [this]() {
        if (--_active_sessions == 0) {
            std::lock_guard<std::mutex> lock(_shutdown_mutex);
            _shutdown_cv.notify_all();
        }
    };
    
    // Rate limiting для соединения (skipped in unprotected mode)
    if (!_unprotected && _rate_limiter && !clean_ip.empty() && clean_ip != "unknown") {
        if (!_rate_limiter->allow_connection(clean_ip)) {
            LOG(WARNING) << "Connection rate limit exceeded for IP: " << clean_ip;
            socket.close(ec);
            decrement_session();
            return;
        }
        _rate_limiter->record_connection(clean_ip);
    }
    
    auto cleanup = [this, clean_ip]() {
        if (_rate_limiter && !clean_ip.empty() && clean_ip != "unknown") {
            _rate_limiter->record_disconnection(clean_ip);
        }
        // Decrement active sessions counter
        if (--_active_sessions == 0) {
            std::lock_guard<std::mutex> lock(_shutdown_mutex);
            _shutdown_cv.notify_all();
        }
    };
    
    try {
        unsigned int request_count = 0;
        bool keep_alive = true;
        
        while (keep_alive && _running) {
            auto start_time = std::chrono::steady_clock::now();
            
            // Check max requests per connection
            if (request_count >= _keep_alive_config.max_requests) {
                VLOG(1) << "Max requests per connection reached for " << client_info;
                break;
            }
            
            beast::flat_buffer buffer;
            http::request_parser<http::string_body> parser;
            parser.body_limit(_rate_limiter_config.max_request_size);
            
            http::read(socket, buffer, parser, ec);
            if (ec) {
                if (ec == beast::http::error::end_of_stream) {
                    VLOG(1) << "Connection closed by client: " << client_info;
                } else if (ec == asio::error::timed_out) {
                    VLOG(1) << "Connection timeout for " << client_info;
                } else {
                    LOG(WARNING) << "Read error from " << client_info << ": " << ec.message();
                }
                break;
            }
            
            http::request<http::string_body> req = parser.release();
            request_count++;
            
            // Determine if we should keep the connection alive
            keep_alive = should_keep_alive(req) && _keep_alive_config.enabled;
            
            // Rate limiting для запроса (skipped in unprotected mode)
            if (!_unprotected && _rate_limiter && !clean_ip.empty() && clean_ip != "unknown") {
                if (!_rate_limiter->allow_request(clean_ip)) {
                    LOG(WARNING) << "Rate limit exceeded for IP: " << clean_ip;
                    http::response<http::string_body> res{http::status::too_many_requests, 11};
                    res.set(http::field::content_type, "application/json");
                    res.body() = R"({"error": "rate_limit_exceeded", "message": "Too many requests"})";
                    set_keep_alive_headers(res, false); // Force close on error
                    res.prepare_payload();
                    http::write(socket, res, ec);
                    break;
                }
                
                size_t request_size = req.body().size();
                if (!_rate_limiter->check_request_size(clean_ip, request_size)) {
                    LOG(WARNING) << "Request size limit exceeded for IP: " << clean_ip;
                    http::response<http::string_body> res{http::status::payload_too_large, 11};
                    res.set(http::field::content_type, "application/json");
                    res.body() = R"({"error": "request_too_large", "message": "Request size exceeds limit"})";
                    set_keep_alive_headers(res, false); // Force close on error
                    res.prepare_payload();
                    http::write(socket, res, ec);
                    break;
                }
                
                _rate_limiter->record_request(clean_ip, request_size);
            }
            
            logging::log_request(
                std::string(req.method_string()),
                std::string(req.target()),
                client_info
            );
            
            // Создаем менеджер файлов с ограничениями загрузки
            upload_limits limits = {
                _upload_limits.max_file_size,
                _upload_limits.max_part_size,
                _upload_limits.max_parts_per_upload,
                _upload_limits.max_temp_storage_total
            };
            file_manager fm(_storage_path, limits);
            request_handler handler(fm, _authenticator, _authorizer);
            handler.set_auth_enabled(_auth_enabled);
            handler.set_authorization_enabled(_authorization_enabled);
            handler.set_cors_config(_cors_config);
            
            // Special handling for metrics endpoint (should go through regular request handler)
            if (req.method() == http::verb::get && std::string(req.target()) == "/metrics") {
                handler.handle_request(
                    std::move(req),
                    [&socket, start_time, keep_alive, this](http::response<http::string_body>&& res) {
                        beast::error_code ec;
                        set_keep_alive_headers(res, keep_alive);
                        http::write(socket, res, ec);
                        if (ec) {
                            LOG(ERROR) << "Write error: " << ec.message();
                        } else {
                            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start_time);
                            VLOG(1) << "Request handled in " << duration.count() << "ms";
                            logging::log_response(static_cast<int>(res.result()));
                        }
                    }
                );
                continue; // Skip the rest of the request processing for this iteration
            }
            
            // === ОСНОВНОЕ ИЗМЕНЕНИЕ: обработка GET-запросов через file_body ===
            if (req.method() == http::verb::get) {
                try {
                    // Пытаемся отправить файл потоково
                    auto file_res = handler.handle_get_file_body(req);
                    http::response<http::file_body> res = std::move(file_res);
                    set_keep_alive_headers(res, keep_alive);
                    http::write(socket, res, ec);
                    if (!ec) {
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start_time);
                        VLOG(1) << "GET (file_body) handled in " << duration.count() << "ms";
                        logging::log_response(static_cast<int>(res.result()));
                    } else {
                        LOG(ERROR) << "Write error (file_body): " << ec.message();
                        break;
                    }
                } catch (const std::exception& e) {
                    // Если файл не найден или другая ошибка — отправляем JSON-ошибку через обычный обработчик
                    LOG(WARNING) << "GET file_body failed, falling back to string_body: " << e.what();
                    auto err_res = handler.handle_get(req);  // возвращает http::response<http::string_body> с ошибкой
                    set_keep_alive_headers(err_res, keep_alive);
                    http::write(socket, err_res, ec);
                    if (ec) {
                        LOG(ERROR) << "Write error (fallback): " << ec.message();
                        break;
                    }
                }
            } else {
                // Для всех остальных методов (PUT, DELETE, POST, OPTIONS) используем старый механизм
                handler.handle_request(
                    std::move(req),
                    [&socket, start_time, keep_alive, this](http::response<http::string_body>&& res) {
                        beast::error_code ec;
                        set_keep_alive_headers(res, keep_alive);
                        http::write(socket, res, ec);
                        if (ec) {
                            LOG(ERROR) << "Write error: " << ec.message();
                        } else {
                            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start_time);
                            VLOG(1) << "Request handled in " << duration.count() << "ms";
                            logging::log_response(static_cast<int>(res.result()));
                        }
                    }
                );
            }
            
            // If we're not keeping alive, break the loop
            if (!keep_alive) {
                VLOG(1) << "Closing connection as requested by client: " << client_info;
                break;
            }
            
            VLOG(2) << "Keeping connection alive for " << client_info << " (request " << request_count << ")";
        }
        
        // Закрываем соединение
        socket.shutdown(tcp::socket::shutdown_send, ec);
        socket.close(ec);
        cleanup();
        
        VLOG(1) << "Connection closed for " << client_info << " after " << request_count << " requests";
        
    } catch (const std::exception& e) {
        LOG(ERROR) << "Session error for " << client_info << ": " << e.what();
        cleanup();
    }
}

void s3_server::handle_ssl_session(ssl::stream<tcp::socket> socket, const std::string& client_ip) {
    // Increment active sessions counter
    ++_active_sessions;
    
    std::string client_info = client_ip;
    beast::error_code ec;
    
    if (client_info.empty() || client_info == "unknown") {
        auto remote_endpoint = socket.next_layer().remote_endpoint(ec);
        if (!ec) {
            client_info = remote_endpoint.address().to_string() +
                         ":" + std::to_string(remote_endpoint.port());
        }
    }
    
    std::string clean_ip = client_info;
    size_t colon_pos = client_info.find(':');
    if (colon_pos != std::string::npos) {
        clean_ip = client_info.substr(0, colon_pos);
    }
    
    // Decrement on exit
    auto decrement_session = [this]() {
        if (--_active_sessions == 0) {
            std::lock_guard<std::mutex> lock(_shutdown_mutex);
            _shutdown_cv.notify_all();
        }
    };
    
    // Rate limiting для соединения (skipped in unprotected mode)
    if (!_unprotected && _rate_limiter && !clean_ip.empty() && clean_ip != "unknown") {
        if (!_rate_limiter->allow_connection(clean_ip)) {
            LOG(WARNING) << "SSL connection rate limit exceeded for IP: " << clean_ip;
            socket.next_layer().close(ec);
            decrement_session();
            return;
        }
        _rate_limiter->record_connection(clean_ip);
    }
    
    auto cleanup = [this, clean_ip]() {
        if (_rate_limiter && !clean_ip.empty() && clean_ip != "unknown") {
            _rate_limiter->record_disconnection(clean_ip);
        }
        // Decrement active sessions counter
        if (--_active_sessions == 0) {
            std::lock_guard<std::mutex> lock(_shutdown_mutex);
            _shutdown_cv.notify_all();
        }
    };
    
    try {
        unsigned int request_count = 0;
        bool keep_alive = true;
        
        while (keep_alive && _running) {
            auto start_time = std::chrono::steady_clock::now();
            
            // Check max requests per connection
            if (request_count >= _keep_alive_config.max_requests) {
                VLOG(1) << "SSL max requests per connection reached for " << client_info;
                break;
            }
            
            beast::flat_buffer buffer;
            http::request_parser<http::string_body> parser;
            parser.body_limit(_rate_limiter_config.max_request_size);
            
            http::read(socket, buffer, parser, ec);
            if (ec) {
                if (ec == beast::http::error::end_of_stream) {
                    VLOG(1) << "SSL connection closed by client: " << client_info;
                } else if (ec == asio::error::timed_out) {
                    VLOG(1) << "SSL connection timeout for " << client_info;
                } else {
                    LOG(WARNING) << "SSL read error from " << client_info << ": " << ec.message();
                }
                break;
            }
            
            http::request<http::string_body> req = parser.release();
            request_count++;
            
            // Determine if we should keep the connection alive
            keep_alive = should_keep_alive(req) && _keep_alive_config.enabled;
            
            // Rate limiting для запроса (skipped in unprotected mode)
            if (!_unprotected && _rate_limiter && !clean_ip.empty() && clean_ip != "unknown") {
                if (!_rate_limiter->allow_request(clean_ip)) {
                    LOG(WARNING) << "SSL rate limit exceeded for IP: " << clean_ip;
                    http::response<http::string_body> res{http::status::too_many_requests, 11};
                    res.set(http::field::content_type, "application/json");
                    res.body() = R"({"error": "rate_limit_exceeded", "message": "Too many requests"})";
                    set_keep_alive_headers(res, false); // Force close on error
                    res.prepare_payload();
                    http::write(socket, res, ec);
                    break;
                }
                
                size_t request_size = req.body().size();
                if (!_rate_limiter->check_request_size(clean_ip, request_size)) {
                    LOG(WARNING) << "SSL request size limit exceeded for IP: " << clean_ip;
                    http::response<http::string_body> res{http::status::payload_too_large, 11};
                    res.set(http::field::content_type, "application/json");
                    res.body() = R"({"error": "request_too_large", "message": "Request size exceeds limit"})";
                    set_keep_alive_headers(res, false); // Force close on error
                    res.prepare_payload();
                    http::write(socket, res, ec);
                    break;
                }
                
                _rate_limiter->record_request(clean_ip, request_size);
            }
            
            logging::log_request(
                std::string(req.method_string()),
                std::string(req.target()),
                client_info
            );
            
            // Создаем менеджер файлов с ограничениями загрузки
            upload_limits limits = {
                _upload_limits.max_file_size,
                _upload_limits.max_part_size,
                _upload_limits.max_parts_per_upload,
                _upload_limits.max_temp_storage_total
            };
            file_manager fm(_storage_path, limits);
            request_handler handler(fm, _authenticator, _authorizer);
            handler.set_auth_enabled(_auth_enabled);
            handler.set_authorization_enabled(_authorization_enabled);
            handler.set_cors_config(_cors_config);
            
            // === АНАЛОГИЧНОЕ ИЗМЕНЕНИЕ для HTTPS ===
            if (req.method() == http::verb::get) {
                try {
                    auto file_res = handler.handle_get_file_body(req);
                    http::response<http::file_body> res = std::move(file_res);
                    set_keep_alive_headers(res, keep_alive);
                    http::write(socket, res, ec);
                    if (!ec) {
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - start_time);
                        VLOG(1) << "SSL GET (file_body) handled in " << duration.count() << "ms";
                        logging::log_response(static_cast<int>(res.result()));
                    } else {
                        LOG(ERROR) << "SSL write error (file_body): " << ec.message();
                        break;
                    }
                } catch (const std::exception& e) {
                    LOG(WARNING) << "SSL GET file_body failed, falling back to string_body: " << e.what();
                    auto err_res = handler.handle_get(req);
                    set_keep_alive_headers(err_res, keep_alive);
                    http::write(socket, err_res, ec);
                    if (ec) {
                        LOG(ERROR) << "SSL write error (fallback): " << ec.message();
                        break;
                    }
                }
            } else {
                handler.handle_request(
                    std::move(req),
                    [&socket, start_time, keep_alive, this](http::response<http::string_body>&& res) {
                        beast::error_code ec;
                        set_keep_alive_headers(res, keep_alive);
                        http::write(socket, res, ec);
                        if (ec) {
                            LOG(ERROR) << "SSL write error: " << ec.message();
                        } else {
                            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start_time);
                            VLOG(1) << "SSL request handled in " << duration.count() << "ms";
                            logging::log_response(static_cast<int>(res.result()));
                        }
                    }
                );
            }
            
            // If we're not keeping alive, break the loop
            if (!keep_alive) {
                VLOG(1) << "SSL closing connection as requested by client: " << client_info;
                break;
            }
            
            VLOG(2) << "SSL keeping connection alive for " << client_info << " (request " << request_count << ")";
        }
        
        socket.shutdown(ec);
        socket.next_layer().shutdown(tcp::socket::shutdown_send, ec);
        socket.next_layer().close(ec);
        cleanup();
        
        VLOG(1) << "SSL connection closed for " << client_info << " after " << request_count << " requests";
        
    } catch (const std::exception& e) {
        LOG(ERROR) << "SSL session error for " << client_info << ": " << e.what();
        cleanup();
    }
}

bool s3_server::should_keep_alive(const http::request<http::string_body>& req) const {
    if (!_keep_alive_config.enabled) {
        return false;
    }
    
    // Check Connection header
    auto it = req.find(http::field::connection);
    if (it != req.end()) {
        std::string connection_value = it->value();
        std::transform(connection_value.begin(), connection_value.end(), connection_value.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        if (connection_value.find("close") != std::string::npos) {
            return false;
        }
        if (connection_value.find("keep-alive") != std::string::npos) {
            return true;
        }
    }
    
    // For HTTP/1.0, keep-alive is disabled by default unless explicitly requested
    if (req.version() == 10) {
        return false;
    }
    
    // For HTTP/1.1, keep-alive is enabled by default unless Connection: close
    return true;
}

void s3_server::set_keep_alive_headers(http::response<http::string_body>& res, bool keep_alive) const {
    if (keep_alive && _keep_alive_config.enabled) {
        res.set(http::field::connection, "keep-alive");
        res.set(http::field::keep_alive, "timeout=" + std::to_string(_keep_alive_config.timeout_seconds));
    } else {
        res.set(http::field::connection, "close");
    }
}

void s3_server::set_keep_alive_headers(http::response<http::file_body>& res, bool keep_alive) const {
    if (keep_alive && _keep_alive_config.enabled) {
        res.set(http::field::connection, "keep-alive");
        res.set(http::field::keep_alive, "timeout=" + std::to_string(_keep_alive_config.timeout_seconds));
    } else {
        res.set(http::field::connection, "close");
    }
}