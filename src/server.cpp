#include "server.hpp"
#include "request_handler.hpp"
#include "logging.hpp"
#include <iostream>
#include <memory>
#include <chrono>
#include <glog/logging.h>

s3_server::s3_server(const std::string& address, unsigned short port, const std::string& storage_path)
    : _address(address)
    , _port(port)
    , _storage_path(storage_path)
    , _acceptor(_io_context)
{
    LOG(INFO) << "S3 server created: " << address << ":" << port;
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
        
        LOG(INFO) << "Starting S3-compatible server on " << _address << ":" << _port;
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
                
                // Запускаем обработку сессии в отдельном потоке
                std::thread([this, socket = std::move(socket)]() mutable {
                    handle_session(std::move(socket));
                }).detach();
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
        http::request<http::string_body> req;
        
        // Читаем запрос
        http::read(socket, buffer, req, ec);
        if (ec) {
            if (ec != beast::http::error::end_of_stream) {
                LOG(WARNING) << "Read error: " << ec.message();
            }
            return;
        }
        
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
            [&socket, start_time](http::response<http::string_body>&& res) {  // Убрали неиспользуемый захват this
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
                    
                    // Логируем ответ
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