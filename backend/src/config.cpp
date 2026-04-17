#include "config.hpp"
#include <fstream>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

// server_config static methods
server_config server_config::from_json(const nlohmann::json& j) {
    return j.get<server_config>();
}

server_config server_config::from_file(const std::string& path) {
    if (!fs::exists(path)) {
        throw std::runtime_error("Configuration file not found: " + path);
    }
    std::ifstream f(path);
    nlohmann::json j;
    f >> j;
    return from_json(j);
}

nlohmann::json server_config::to_json() const {
    return nlohmann::json(*this);
}

// server_config serialization
void from_json(const nlohmann::json& j, server_config& cfg) {
    if (j.contains("address")) j.at("address").get_to(cfg.address);
    if (j.contains("port")) j.at("port").get_to(cfg.port);
    if (j.contains("storage_path")) j.at("storage_path").get_to(cfg.storage_path);
    if (j.contains("keys_file")) j.at("keys_file").get_to(cfg.keys_file);
    if (j.contains("users_file")) j.at("users_file").get_to(cfg.users_file);
    if (j.contains("acls_file")) j.at("acls_file").get_to(cfg.acls_file);
    if (j.contains("enable_auth")) j.at("enable_auth").get_to(cfg.enable_auth);
    if (j.contains("enable_ssl")) j.at("enable_ssl").get_to(cfg.enable_ssl);
    if (j.contains("use_letsencrypt")) j.at("use_letsencrypt").get_to(cfg.use_letsencrypt);
    if (j.contains("letsencrypt_dir")) j.at("letsencrypt_dir").get_to(cfg.letsencrypt_dir);
    
    // SSL config
    if (j.contains("ssl") && !j.at("ssl").is_null()) {
        s3_server::ssl_config ssl;
        j.at("ssl").get_to(ssl);
        cfg.ssl_cfg = ssl;
    } else {
        cfg.ssl_cfg = std::nullopt;
    }
    
    // CORS config
    if (j.contains("cors") && !j.at("cors").is_null()) {
        s3_server::cors_config cors;
        j.at("cors").get_to(cors);
        cfg.cors_cfg = cors;
    } else {
        cfg.cors_cfg = std::nullopt;
    }
    
    // Upload limits
    if (j.contains("upload_limits")) {
        j.at("upload_limits").get_to(cfg.upload_limits);
    }
    
    // Keep-alive
    if (j.contains("keep_alive")) {
        j.at("keep_alive").get_to(cfg.keep_alive);
    }
    
    // Rate limiter
    if (j.contains("rate_limiter")) {
        j.at("rate_limiter").get_to(cfg.rate_limiter);
    }
    
    // Logging
    if (j.contains("logging")) {
        j.at("logging").get_to(cfg.logging);
    }
}

void to_json(nlohmann::json& j, const server_config& cfg) {
    j["address"] = cfg.address;
    j["port"] = cfg.port;
    j["storage_path"] = cfg.storage_path;
    j["keys_file"] = cfg.keys_file;
    j["users_file"] = cfg.users_file;
    j["acls_file"] = cfg.acls_file;
    j["enable_auth"] = cfg.enable_auth;
    j["enable_ssl"] = cfg.enable_ssl;
    j["use_letsencrypt"] = cfg.use_letsencrypt;
    j["letsencrypt_dir"] = cfg.letsencrypt_dir;
    
    if (cfg.ssl_cfg.has_value()) {
        j["ssl"] = cfg.ssl_cfg.value();
    } else {
        j["ssl"] = nullptr;
    }
    
    if (cfg.cors_cfg.has_value()) {
        j["cors"] = cfg.cors_cfg.value();
    } else {
        j["cors"] = nullptr;
    }
    
    j["upload_limits"] = cfg.upload_limits;
    j["keep_alive"] = cfg.keep_alive;
    j["rate_limiter"] = cfg.rate_limiter;
    j["logging"] = cfg.logging;
}

// upload_limits_config
void from_json(const nlohmann::json& j, upload_limits_config& cfg) {
    if (j.contains("max_file_size")) j.at("max_file_size").get_to(cfg.max_file_size);
    if (j.contains("max_part_size")) j.at("max_part_size").get_to(cfg.max_part_size);
    if (j.contains("max_parts_per_upload")) j.at("max_parts_per_upload").get_to(cfg.max_parts_per_upload);
    if (j.contains("max_temp_storage_total")) j.at("max_temp_storage_total").get_to(cfg.max_temp_storage_total);
}

void to_json(nlohmann::json& j, const upload_limits_config& cfg) {
    j["max_file_size"] = cfg.max_file_size;
    j["max_part_size"] = cfg.max_part_size;
    j["max_parts_per_upload"] = cfg.max_parts_per_upload;
    j["max_temp_storage_total"] = cfg.max_temp_storage_total;
}

// keep_alive_config
void from_json(const nlohmann::json& j, keep_alive_config& cfg) {
    if (j.contains("enabled")) j.at("enabled").get_to(cfg.enabled);
    if (j.contains("timeout_seconds")) j.at("timeout_seconds").get_to(cfg.timeout_seconds);
    if (j.contains("max_requests")) j.at("max_requests").get_to(cfg.max_requests);
}

void to_json(nlohmann::json& j, const keep_alive_config& cfg) {
    j["enabled"] = cfg.enabled;
    j["timeout_seconds"] = cfg.timeout_seconds;
    j["max_requests"] = cfg.max_requests;
}

// ssl_config
void from_json(const nlohmann::json& j, s3_server::ssl_config& cfg) {
    if (j.contains("cert_file")) j.at("cert_file").get_to(cfg.cert_file);
    if (j.contains("private_key")) j.at("private_key").get_to(cfg.private_key);
    if (j.contains("dh_file") && !j.at("dh_file").is_null()) {
        cfg.dh_file = j.at("dh_file").get<std::string>();
    } else {
        cfg.dh_file = std::nullopt;
    }
    if (j.contains("ca_file") && !j.at("ca_file").is_null()) {
        cfg.ca_file = j.at("ca_file").get<std::string>();
    } else {
        cfg.ca_file = std::nullopt;
    }
    if (j.contains("ca_path") && !j.at("ca_path").is_null()) {
        cfg.ca_path = j.at("ca_path").get<std::string>();
    } else {
        cfg.ca_path = std::nullopt;
    }
    if (j.contains("verify_client")) j.at("verify_client").get_to(cfg.verify_client);
}

void to_json(nlohmann::json& j, const s3_server::ssl_config& cfg) {
    j["cert_file"] = cfg.cert_file;
    j["private_key"] = cfg.private_key;
    if (cfg.dh_file.has_value()) {
        j["dh_file"] = cfg.dh_file.value();
    } else {
        j["dh_file"] = nullptr;
    }
    if (cfg.ca_file.has_value()) {
        j["ca_file"] = cfg.ca_file.value();
    } else {
        j["ca_file"] = nullptr;
    }
    if (cfg.ca_path.has_value()) {
        j["ca_path"] = cfg.ca_path.value();
    } else {
        j["ca_path"] = nullptr;
    }
    j["verify_client"] = cfg.verify_client;
}

// cors_config
void from_json(const nlohmann::json& j, s3_server::cors_config& cfg) {
    if (j.contains("allowed_origins")) j.at("allowed_origins").get_to(cfg.allowed_origins);
    if (j.contains("allowed_methods")) j.at("allowed_methods").get_to(cfg.allowed_methods);
    if (j.contains("allowed_headers")) j.at("allowed_headers").get_to(cfg.allowed_headers);
    if (j.contains("exposed_headers")) j.at("exposed_headers").get_to(cfg.exposed_headers);
    if (j.contains("allow_credentials")) j.at("allow_credentials").get_to(cfg.allow_credentials);
    if (j.contains("max_age")) j.at("max_age").get_to(cfg.max_age);
}

void to_json(nlohmann::json& j, const s3_server::cors_config& cfg) {
    j["allowed_origins"] = cfg.allowed_origins;
    j["allowed_methods"] = cfg.allowed_methods;
    j["allowed_headers"] = cfg.allowed_headers;
    j["exposed_headers"] = cfg.exposed_headers;
    j["allow_credentials"] = cfg.allow_credentials;
    j["max_age"] = cfg.max_age;
}

// rate_limiter_config
void from_json(const nlohmann::json& j, rate_limiter_config& cfg) {
    if (j.contains("max_requests_per_minute")) j.at("max_requests_per_minute").get_to(cfg.max_requests_per_minute);
    if (j.contains("max_requests_per_hour")) j.at("max_requests_per_hour").get_to(cfg.max_requests_per_hour);
    if (j.contains("max_burst_size")) j.at("max_burst_size").get_to(cfg.max_burst_size);
    if (j.contains("max_connections_per_ip")) j.at("max_connections_per_ip").get_to(cfg.max_connections_per_ip);
    if (j.contains("max_connection_rate")) j.at("max_connection_rate").get_to(cfg.max_connection_rate);
    if (j.contains("max_request_size")) j.at("max_request_size").get_to(cfg.max_request_size);
    if (j.contains("max_upload_size")) j.at("max_upload_size").get_to(cfg.max_upload_size);
    if (j.contains("window_seconds")) j.at("window_seconds").get_to(cfg.window_seconds);
    if (j.contains("ban_duration")) j.at("ban_duration").get_to(cfg.ban_duration);
    if (j.contains("enable_ddos_protection")) j.at("enable_ddos_protection").get_to(cfg.enable_ddos_protection);
    if (j.contains("ddos_threshold")) j.at("ddos_threshold").get_to(cfg.ddos_threshold);
    if (j.contains("ddos_window")) j.at("ddos_window").get_to(cfg.ddos_window);
}

void to_json(nlohmann::json& j, const rate_limiter_config& cfg) {
    j["max_requests_per_minute"] = cfg.max_requests_per_minute;
    j["max_requests_per_hour"] = cfg.max_requests_per_hour;
    j["max_burst_size"] = cfg.max_burst_size;
    j["max_connections_per_ip"] = cfg.max_connections_per_ip;
    j["max_connection_rate"] = cfg.max_connection_rate;
    j["max_request_size"] = cfg.max_request_size;
    j["max_upload_size"] = cfg.max_upload_size;
    j["window_seconds"] = cfg.window_seconds;
    j["ban_duration"] = cfg.ban_duration;
    j["enable_ddos_protection"] = cfg.enable_ddos_protection;
    j["ddos_threshold"] = cfg.ddos_threshold;
    j["ddos_window"] = cfg.ddos_window;
}

// logging_config
void from_json(const nlohmann::json& j, server_config::logging_config& cfg) {
    if (j.contains("log_level")) j.at("log_level").get_to(cfg.log_level);
    if (j.contains("log_dir")) j.at("log_dir").get_to(cfg.log_dir);
}

void to_json(nlohmann::json& j, const server_config::logging_config& cfg) {
    j["log_level"] = cfg.log_level;
    j["log_dir"] = cfg.log_dir;
}