#include "server.hpp"
#include "request_handler.hpp"
#include <iostream>
#include <memory>

s3_server::s3_server(const std::string& address, unsigned short port, const std::string& storage_path)
    : _address(address)
    , _port(port)
    , _storage_path(storage_path)
    , _acceptor(_io_context)
{
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
    
    // Открываем acceptor
    tcp::endpoint endpoint(asio::ip::make_address(_address), _port);
    _acceptor.open(endpoint.protocol());
    _acceptor.set_option(asio::socket_base::reuse_address(true));
    _acceptor.bind(endpoint);
    _acceptor.listen(asio::socket_base::max_listen_connections);
    
    std::cout << "Starting S3-compatible server on " << _address << ":" << _port << std::endl;
    std::cout << "Storage path: " << _storage_path << std::endl;
    std::cout << "Using " << threads << " threads" << std::endl;
    
    _running = true;
    
    // Запускаем первую асинхронную операцию принятия соединения
    do_accept();
    
    // Запускаем пул потоков
    _threads.reserve(threads);
    for (std::size_t i = 0; i < threads; ++i) {
        _threads.emplace_back([this] {
            _io_context.run();
        });
    }
}

void s3_server::stop()
{
    _running = false;
    _io_context.stop();
    
    for (auto& thread : _threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    _threads.clear();
    
    std::cout << "Server stopped" << std::endl;
}

void s3_server::do_accept()
{
    if (!_running) {
        return;
    }
    
    _acceptor.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                // Запускаем обработку сессии в отдельном потоке io_context
                std::thread([this, socket = std::move(socket)]() mutable {
                    handle_session(std::move(socket));
                }).detach();
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
    try {
        beast::error_code ec;
        
        // Создаем объекты для чтения/записи
        beast::flat_buffer buffer;
        http::request<http::string_body> req;
        
        // Читаем запрос
        http::read(socket, buffer, req, ec);
        if (ec) {
            return;
        }
        
        // Создаем менеджер файлов и обработчик запросов
        file_manager fm(_storage_path);
        request_handler handler(fm);
        
        // Обрабатываем запрос
        handler.handle_request(
            std::move(req),
            [&socket](http::response<http::string_body>&& res) {
                // Отправляем ответ
                beast::error_code ec;
                http::write(socket, res, ec);
                
                // Закрываем соединение
                socket.shutdown(tcp::socket::shutdown_send, ec);
                socket.close(ec);
            }
        );
    } catch (const std::exception& e) {
        std::cerr << "Session error: " << e.what() << std::endl;
    }
}