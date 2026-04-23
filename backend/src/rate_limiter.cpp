#include "rate_limiter.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

using namespace std::chrono;

rate_limiter::rate_limiter(const rate_limiter_config& cfg)
    : _config(cfg)
{
}

bool rate_limiter::allow_request(const std::string& ip_address)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto& data = _ip_data_map[ip_address];
    
    // Check if IP is banned
    if (data.banned) {
        if (now() < data.ban_until) {
            return false; // Still banned
        } else {
            data.banned = false; // Ban expired
        }
    }
    
    // Clean up old request timestamps
    cleanup_old_requests(data);
    
    // Check request rate limits
    size_t requests_last_minute = 0;
    size_t requests_last_hour = 0;
    
    auto one_minute_ago = now() - minutes(1);
    auto one_hour_ago = now() - hours(1);
    
    for (const auto& timestamp : data.request_timestamps) {
        if (timestamp > one_minute_ago) {
            requests_last_minute++;
        }
        if (timestamp > one_hour_ago) {
            requests_last_hour++;
        }
    }
    
    // Check limits
    if (requests_last_minute >= _config.max_requests_per_minute) {
        // Too many requests in last minute
        if (_config.enable_ddos_protection && check_ddos_attack(ip_address, data)) {
            // DDoS attack detected, ban the IP
            data.banned = true;
            data.ban_until = now() + seconds(_config.ban_duration);
            return false;
        }
        return false;
    }
    
    if (requests_last_hour >= _config.max_requests_per_hour) {
        return false;
    }
    
    // Check burst limit
    if (data.request_timestamps.size() >= _config.max_burst_size) {
        // Check if the oldest request in burst window is too recent
        auto oldest_in_burst = *std::min_element(data.request_timestamps.begin(), data.request_timestamps.end());
        if (!is_expired(oldest_in_burst, seconds(_config.window_seconds))) {
            return false;
        }
    }
    
    return true;
}

bool rate_limiter::allow_connection(const std::string& ip_address)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto& data = _ip_data_map[ip_address];
    
    // Check concurrent connections limit
    if (data.current_connections >= _config.max_connections_per_ip) {
        return false;
    }
    
    // Clean up old connection timestamps
    cleanup_old_connections(data);
    
    // Check connection rate (new connections per second)
    auto one_second_ago = now() - seconds(1);
    size_t connections_last_second = 0;
    
    for (const auto& timestamp : data.connection_timestamps) {
        if (timestamp > one_second_ago) {
            connections_last_second++;
        }
    }
    
    if (connections_last_second >= _config.max_connection_rate) {
        return false;
    }
    
    return true;
}

void rate_limiter::record_request(const std::string& ip_address, size_t request_size)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto& data = _ip_data_map[ip_address];
    data.request_timestamps.push_back(now());
    data.total_requests++;
    
    // Track bandwidth
    data.bytes_uploaded += request_size;
    
    // For DDoS detection
    if (_config.enable_ddos_protection) {
        data.ddos_timestamps.push_back(now());
        cleanup_old_ddos_entries(data);
    }
}

void rate_limiter::record_connection(const std::string& ip_address)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto& data = _ip_data_map[ip_address];
    data.current_connections++;
    data.connection_timestamps.push_back(now());
}

void rate_limiter::record_disconnection(const std::string& ip_address)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _ip_data_map.find(ip_address);
    if (it != _ip_data_map.end()) {
        if (it->second.current_connections > 0) {
            it->second.current_connections--;
        }
    }
}

bool rate_limiter::check_request_size(const std::string& ip_address, size_t request_size)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    // Check per-request size limit
    if (request_size > _config.max_request_size) {
        return false;
    }
    
    // Check total upload size (could be extended to have daily/monthly limits)
    // For now, just check against max_upload_size
    auto& data = _ip_data_map[ip_address];
    if (data.bytes_uploaded + request_size > _config.max_upload_size) {
        return false;
    }
    
    return true;
}

std::optional<rate_limiter::ip_stats> rate_limiter::get_ip_stats(const std::string& ip_address)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _ip_data_map.find(ip_address);
    if (it == _ip_data_map.end()) {
        return std::nullopt;
    }
    
    const auto& data = it->second;
    
    // Calculate requests in last minute and hour
    size_t requests_last_minute = 0;
    size_t requests_last_hour = 0;
    
    auto one_minute_ago = now() - minutes(1);
    auto one_hour_ago = now() - hours(1);
    
    for (const auto& timestamp : data.request_timestamps) {
        if (timestamp > one_minute_ago) {
            requests_last_minute++;
        }
        if (timestamp > one_hour_ago) {
            requests_last_hour++;
        }
    }
    
    ip_stats stats;
    stats.requests_last_minute = requests_last_minute;
    stats.requests_last_hour = requests_last_hour;
    stats.current_connections = data.current_connections;
    stats.total_requests = data.total_requests;
    stats.is_banned = data.banned;
    stats.ban_until = data.ban_until;
    
    return stats;
}

void rate_limiter::cleanup()
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    // Remove IPs with no recent activity (older than 24 hours)
    // auto one_day_ago = now() - hours(24); // unused
    
    for (auto it = _ip_data_map.begin(); it != _ip_data_map.end(); ) {
        auto& data = it->second;
        
        // Clean up old timestamps
        cleanup_old_requests(data);
        cleanup_old_connections(data);
        cleanup_old_ddos_entries(data);
        
        // Remove IP if no recent activity and no active connections
        if (data.request_timestamps.empty() && 
            data.connection_timestamps.empty() &&
            data.current_connections == 0 &&
            data.ddos_timestamps.empty() &&
            !data.banned) {
            it = _ip_data_map.erase(it);
        } else {
            ++it;
        }
    }
}

void rate_limiter::reset()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _ip_data_map.clear();
}

void rate_limiter::cleanup_old_requests(ip_data& data)
{
    //auto one_hour_ago = now() - hours(1);
    data.request_timestamps.erase(
        std::remove_if(data.request_timestamps.begin(), data.request_timestamps.end(),
            [this/*, one_hour_ago*/](const auto& timestamp) {
                return is_expired(timestamp, hours(1));
            }),
        data.request_timestamps.end()
    );
}

void rate_limiter::cleanup_old_connections(ip_data& data)
{
    //auto one_minute_ago = now() - minutes(1);
    data.connection_timestamps.erase(
        std::remove_if(data.connection_timestamps.begin(), data.connection_timestamps.end(),
            [this/*, one_minute_ago*/](const auto& timestamp) {
                return is_expired(timestamp, minutes(1));
            }),
        data.connection_timestamps.end()
    );
}

void rate_limiter::cleanup_old_ddos_entries(ip_data& data)
{
    //auto ddos_window_ago = now() - seconds(_config.ddos_window);
    data.ddos_timestamps.erase(
        std::remove_if(data.ddos_timestamps.begin(), data.ddos_timestamps.end(),
            [this/*, ddos_window_ago*/](const auto& timestamp) {
                return is_expired(timestamp, seconds(_config.ddos_window));
            }),
        data.ddos_timestamps.end()
    );
}

bool rate_limiter::check_ddos_attack(const std::string& ip_address, ip_data& data)
{
    if (!_config.enable_ddos_protection) {
        return false;
    }
    
    cleanup_old_ddos_entries(data);
    
    // Check if requests in DDoS window exceed threshold
    if (data.ddos_timestamps.size() >= _config.ddos_threshold) {
        std::cout << "DDoS attack detected from IP: " << ip_address 
                  << " (" << data.ddos_timestamps.size() 
                  << " requests in " << _config.ddos_window << " seconds)" << std::endl;
        return true;
    }
    
    return false;
}