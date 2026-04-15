#pragma once

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <memory>
#include <optional>
#include <boost/asio.hpp>

namespace asio = boost::asio;

struct rate_limiter_config {
    // Request rate limiting
    size_t max_requests_per_minute = 60;      // 1 request per second
    size_t max_requests_per_hour = 3600;      // 60 requests per minute
    size_t max_burst_size = 10;               // Allow burst of 10 requests
    
    // Connection limiting
    size_t max_connections_per_ip = 10;       // Max concurrent connections per IP
    size_t max_connection_rate = 100;         // Max new connections per second per IP (increased for tests)
    
    // Request size limits
    size_t max_request_size = 100 * 1024 * 1024;  // 100 MB
    size_t max_upload_size = 1 * 1024 * 1024 * 1024; // 1 GB
    
    // Time windows (in seconds)
    int window_seconds = 60;                  // Sliding window for rate limiting
    int ban_duration = 300;                   // 5 minutes ban for violators
    
    // DDoS protection
    bool enable_ddos_protection = true;
    size_t ddos_threshold = 100;              // Requests per second to trigger DDoS protection
    size_t ddos_window = 10;                  // 10-second window for DDoS detection
};

class rate_limiter {
public:
    rate_limiter(const rate_limiter_config& cfg = rate_limiter_config());
    
    // Check if IP is allowed to make a request
    bool allow_request(const std::string& ip_address);
    
    // Check if IP is allowed to establish a new connection
    bool allow_connection(const std::string& ip_address);
    
    // Record a completed request
    void record_request(const std::string& ip_address, size_t request_size = 0);
    
    // Record connection established
    void record_connection(const std::string& ip_address);
    
    // Record connection closed
    void record_disconnection(const std::string& ip_address);
    
    // Check request size limit
    bool check_request_size(const std::string& ip_address, size_t request_size);
    
    // Get current statistics for an IP
    struct ip_stats {
        size_t requests_last_minute;
        size_t requests_last_hour;
        size_t current_connections;
        size_t total_requests;
        bool is_banned;
        std::chrono::system_clock::time_point ban_until;
    };
    
    std::optional<ip_stats> get_ip_stats(const std::string& ip_address);
    
    // Clear old entries (garbage collection)
    void cleanup();
    
    // Reset all limits (for testing)
    void reset();
    
private:
    struct ip_data {
        // Request tracking
        std::vector<std::chrono::system_clock::time_point> request_timestamps;
        size_t total_requests = 0;
        
        // Connection tracking
        size_t current_connections = 0;
        std::vector<std::chrono::system_clock::time_point> connection_timestamps;
        
        // Bandwidth tracking
        size_t bytes_uploaded = 0;
        size_t bytes_downloaded = 0;
        
        // Ban status
        bool banned = false;
        std::chrono::system_clock::time_point ban_until;
        
        // DDoS detection
        std::vector<std::chrono::system_clock::time_point> ddos_timestamps;
    };
    
    rate_limiter_config _config;
    std::unordered_map<std::string, ip_data> _ip_data_map;
    std::mutex _mutex;
    
    // Helper methods
    void cleanup_old_requests(ip_data& data);
    void cleanup_old_connections(ip_data& data);
    void cleanup_old_ddos_entries(ip_data& data);
    bool check_ddos_attack(const std::string& ip_address, ip_data& data);
    
    // Time utilities
    std::chrono::system_clock::time_point now() const {
        return std::chrono::system_clock::now();
    }
    
    template<typename Duration>
    bool is_expired(const std::chrono::system_clock::time_point& timestamp, Duration max_age) const {
        return now() - timestamp > max_age;
    }
};