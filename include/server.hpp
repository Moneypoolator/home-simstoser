#pragma once

#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <optional>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/asio/ssl.hpp>
#include "file_manager.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;
namespace ssl = asio::ssl;

class s3_server {
public:
    struct ssl_config {
        std::string cert_file;      // Путь к файлу сертификата
        std::string private_key;    // Путь к приватному ключу
        std::optional<std::string> dh_file;  // Путь к файлу Diffie-Hellman параметров (опционально)
        bool verify_client = false; // Требовать проверку клиента
    };

    s3_server(const std::string& address, 
              unsigned short port, 
              const std::string& storage_path,
              std::optional<ssl_config> ssl_cfg = std::nullopt);
    
    ~s3_server();
    
    void run(std::size_t threads = std::thread::hardware_concurrency());
    void stop();
    
    bool is_ssl_enabled() const { return _ssl_enabled; }
    
private:
    std::string _address;
    unsigned short _port;
    std::string _storage_path;
    
    asio::io_context _io_context;
    tcp::acceptor _acceptor;
    std::vector<std::thread> _threads;
    std::atomic<bool> _running{true};
    
    std::optional<ssl_config> _ssl_config;
    bool _ssl_enabled = false;
    
    void do_accept();
    
    // Обработка обычных (HTTP) соединений
    void handle_session(tcp::socket socket);
    
    // Обработка зашифрованных (HTTPS) соединений
    void handle_ssl_session(ssl::stream<tcp::socket> socket);
    
    // Настройка SSL контекста
    std::shared_ptr<ssl::context> setup_ssl_context();
};