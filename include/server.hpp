#pragma once

#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;
using tcp = asio::ip::tcp;

class s3_server {
public:
    s3_server(const std::string& address, unsigned short port, const std::string& storage_path);
    ~s3_server();
    
    void run(std::size_t threads = std::thread::hardware_concurrency());
    void stop();
    
private:
    std::string _address;
    unsigned short _port;
    std::string _storage_path;
    
    asio::io_context _io_context;
    tcp::acceptor _acceptor;
    std::vector<std::thread> _threads;
    std::atomic<bool> _running{true};
    
    void do_accept();
    void handle_session(tcp::socket socket);
};