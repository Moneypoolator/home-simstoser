#pragma once

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <memory>
#include <optional>
#include <condition_variable>
#include <mutex>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/asio/ssl.hpp>

#include "authenticator.hpp"
#include "authorizer.hpp"
#include "rate_limiter.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
using tcp = asio::ip::tcp;
// namespace ssl = asio::ssl;

    struct upload_limits_config {
        size_t max_file_size = 1024 * 1024 * 1024;
        size_t max_part_size = 100 * 1024 * 1024;
        size_t max_parts_per_upload = 10000;
        size_t max_temp_storage_total = 10ULL * 1024 * 1024 * 1024;
    };

    struct keep_alive_config {
        bool enabled = true;
        unsigned int timeout_seconds = 15;  // Timeout for idle connections
        unsigned int max_requests = 100;    // Maximum requests per connection
    };

class s3_server {
public:
    struct ssl_config {
        std::string cert_file;
        std::string private_key;
        std::optional<std::string> dh_file;
        bool verify_client = false;
    };

    struct cors_config {
        std::vector<std::string> allowed_origins = {"*"};
        std::vector<std::string> allowed_methods = {"GET", "POST", "PUT", "DELETE", "OPTIONS", "HEAD"};
        std::vector<std::string> allowed_headers = {"Content-Type", "Authorization", "X-Amz-Date", "X-Amz-Security-Token", "X-Requested-With", "X-Access-Key"};
        std::vector<std::string> exposed_headers = {"ETag", "X-File-Size", "X-Upload-Id"};
        bool allow_credentials = false;
        int max_age = 86400; // 24 hours in seconds
        
        // Helper method to check if an origin is allowed
        bool is_origin_allowed(const std::string& origin) const {
            if (allowed_origins.empty()) return false;
            if (allowed_origins.size() == 1 && allowed_origins[0] == "*") return true;
            return std::find(allowed_origins.begin(), allowed_origins.end(), origin) != allowed_origins.end();
        }
        
        // Helper method to get allowed origins as a comma-separated string
        std::string get_allowed_origins_header(const std::string& request_origin = "") const {
            if (allowed_origins.empty()) return "";
            if (allowed_origins.size() == 1 && allowed_origins[0] == "*") return "*";
            
            // If a specific origin is requested and it's allowed, return it
            if (!request_origin.empty() && is_origin_allowed(request_origin)) {
                return request_origin;
            }
            
            // Otherwise return all allowed origins
            std::string result;
            for (size_t i = 0; i < allowed_origins.size(); ++i) {
                if (i > 0) result += ", ";
                result += allowed_origins[i];
            }
            return result;
        }
        
        // Helper method to get allowed methods as a comma-separated string
        std::string get_allowed_methods_header() const {
            std::string result;
            for (size_t i = 0; i < allowed_methods.size(); ++i) {
                if (i > 0) result += ", ";
                result += allowed_methods[i];
            }
            return result;
        }
        
        // Helper method to get allowed headers as a comma-separated string
        std::string get_allowed_headers_header() const {
            std::string result;
            for (size_t i = 0; i < allowed_headers.size(); ++i) {
                if (i > 0) result += ", ";
                result += allowed_headers[i];
            }
            return result;
        }
        
        // Helper method to get exposed headers as a comma-separated string
        std::string get_exposed_headers_header() const {
            std::string result;
            for (size_t i = 0; i < exposed_headers.size(); ++i) {
                if (i > 0) result += ", ";
                result += exposed_headers[i];
            }
            return result;
        }
    };

    s3_server(const std::string& address,
              unsigned short port,
              const std::string& storage_path,
              const std::string& keys_file = "",
              const std::string& users_file = "",
              std::optional<ssl_config> ssl_cfg = std::nullopt,
              std::optional<cors_config> cors_cfg = std::nullopt,
              upload_limits_config upload_limits = upload_limits_config(),
              keep_alive_config keep_alive = keep_alive_config(),
              rate_limiter_config rate_limiter = rate_limiter_config());
    
    ~s3_server();
    
    void run(std::size_t threads = std::thread::hardware_concurrency());
    void stop();
    
    bool is_ssl_enabled() const { return _ssl_enabled; }
    bool is_auth_enabled() const { return _auth_enabled; }
    
private:
    std::string _address;
    unsigned short _port;
    std::string _storage_path;
    std::string _keys_file;
    std::string _users_file;
    asio::io_context _io_context;
    tcp::acceptor _acceptor;
    asio::steady_timer _cleanup_timer;
    std::vector<std::thread> _threads;
    std::atomic<bool> _running{true};
    std::atomic<int> _active_sessions{0};
    std::mutex _shutdown_mutex;
    std::condition_variable _shutdown_cv;
    bool _shutdown_requested{false};
    
    std::optional<ssl_config> _ssl_config;
    std::optional<cors_config> _cors_config;
    bool _ssl_enabled = false;
    bool _auth_enabled = false;
    bool _authorization_enabled = false;
    std::unique_ptr<authenticator> _authenticator;
    std::unique_ptr<authorizer> _authorizer;
    std::unique_ptr<rate_limiter> _rate_limiter;

    upload_limits_config _upload_limits;
    keep_alive_config _keep_alive_config;
    rate_limiter_config _rate_limiter_config;
    
    // Rate limiter cleanup timer
    static constexpr std::chrono::minutes CLEANUP_INTERVAL{5};
    void start_cleanup_timer();
    
    void do_accept();
    void handle_session(tcp::socket socket, const std::string& client_ip = "");
    void handle_ssl_session(asio::ssl::stream<tcp::socket> socket, const std::string& client_ip = "");
    std::shared_ptr<asio::ssl::context> setup_ssl_context();
    
    // Helper methods for keep-alive support
    bool should_keep_alive(const http::request<http::string_body>& req) const;
    void set_keep_alive_headers(http::response<http::string_body>& res, bool keep_alive) const;
    void set_keep_alive_headers(http::response<http::file_body>& res, bool keep_alive) const;
};