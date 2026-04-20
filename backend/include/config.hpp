#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>

#include "server.hpp"
#include "rate_limiter.hpp"

struct server_config {
    // Server basic settings
    std::string address = "0.0.0.0";
    unsigned short port = 9000;
    std::string storage_path = "./storage";
    std::string keys_file = "./access_keys.csv";
    std::string users_file = "./users.csv";
    std::string acls_file = "./acls.json";
    bool enable_auth = true;
    bool enable_ssl = false;
    bool use_letsencrypt = false;
    std::string letsencrypt_dir = "";
    
    // SSL configuration (if enable_ssl is true)
    std::optional<s3_server::ssl_config> ssl_cfg;
    
    // CORS configuration
    std::optional<s3_server::cors_config> cors_cfg;
    
    // Upload limits
    upload_limits_config upload_limits;
    
    // Keep-alive configuration
    keep_alive_config keep_alive;
    
    // Rate limiter configuration
    rate_limiter_config rate_limiter;
    
    // Cache configuration
    cache_config cache;
    
    // Logging configuration
    struct logging_config {
        int log_level = 0; // glog v level
        std::string log_dir = "";
    } logging;
    
    // Helper method to load from JSON
    static server_config from_json(const nlohmann::json& j);
    
    // Helper method to load from file
    static server_config from_file(const std::string& path);
    
    // Convert to JSON (for saving)
    nlohmann::json to_json() const;
};

// JSON serialization/deserialization functions
void from_json(const nlohmann::json& j, server_config& cfg);
void to_json(nlohmann::json& j, const server_config& cfg);

// Sub-config serialization
void from_json(const nlohmann::json& j, upload_limits_config& cfg);
void to_json(nlohmann::json& j, const upload_limits_config& cfg);

void from_json(const nlohmann::json& j, keep_alive_config& cfg);
void to_json(nlohmann::json& j, const keep_alive_config& cfg);

void from_json(const nlohmann::json& j, s3_server::ssl_config& cfg);
void to_json(nlohmann::json& j, const s3_server::ssl_config& cfg);

void from_json(const nlohmann::json& j, s3_server::cors_config& cfg);
void to_json(nlohmann::json& j, const s3_server::cors_config& cfg);

void from_json(const nlohmann::json& j, rate_limiter_config& cfg);
void to_json(nlohmann::json& j, const rate_limiter_config& cfg);

void from_json(const nlohmann::json& j, server_config::logging_config& cfg);
void to_json(nlohmann::json& j, const server_config::logging_config& cfg);