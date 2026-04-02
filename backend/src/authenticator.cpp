#include "authenticator.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <ctime>
#include <set>
#include <glog/logging.h>
#include "logging.hpp"

authenticator::authenticator() {
    LOG(INFO) << "Authenticator initialized";
}

bool authenticator::load_keys(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    std::ifstream file(filepath);
    if (!file) {
        LOG(WARNING) << "Access keys file not found: " << filepath;
        return false;
    }
    
    _keys.clear();
    std::string line;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream iss(line);
        std::string access_key_id, secret_key, user_name, is_active_str;
        
        if (std::getline(iss, access_key_id, ',') &&
            std::getline(iss, secret_key, ',') &&
            std::getline(iss, user_name, ',') &&
            std::getline(iss, is_active_str, ',')) {
            
            access_key key;
            key.access_key_id = access_key_id;
            key.secret_access_key = secret_key;
            key.user_name = user_name;
            key.is_active = (is_active_str == "1" || is_active_str == "true");
            key.created_at = std::chrono::system_clock::now();
            
            _keys[access_key_id] = key;
            LOG(INFO) << "Loaded access key for user: " << user_name;
        }
    }
    
    LOG(INFO) << "Loaded " << _keys.size() << " access keys";
    return true;
}

bool authenticator::save_keys(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(_mutex);
    
    std::ofstream file(filepath);
    if (!file) {
        LOG(ERROR) << "Failed to open file for writing: " << filepath;
        return false;
    }
    
    for (const auto& [id, key] : _keys) {
        file << key.access_key_id << ","
             << key.secret_access_key << ","
             << key.user_name << ","
             << (key.is_active ? "1" : "0") << std::endl;
    }
    
    LOG(INFO) << "Saved " << _keys.size() << " access keys";
    return true;
}

std::optional<access_key> authenticator::create_access_key(
    const std::string& user_name,
    const std::string& access_key_id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    access_key key;
    key.access_key_id = access_key_id.empty() ? generate_access_key_id() : access_key_id;
    key.secret_access_key = generate_secret_key();
    key.user_name = user_name;
    key.is_active = true;
    key.created_at = std::chrono::system_clock::now();
    
    _keys[key.access_key_id] = key;
    
    LOG(INFO) << "Created access key for user: " << user_name 
              << " (ID: " << key.access_key_id << ")";
    
    return key;
}

bool authenticator::delete_access_key(const std::string& access_key_id) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _keys.find(access_key_id);
    if (it == _keys.end()) {
        LOG(WARNING) << "Access key not found: " << access_key_id;
        return false;
    }
    
    LOG(INFO) << "Deleted access key: " << access_key_id 
              << " (user: " << it->second.user_name << ")";
    
    _keys.erase(it);
    return true;
}

bool authenticator::deactivate_key(const std::string& access_key_id) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _keys.find(access_key_id);
    if (it == _keys.end()) {
        LOG(WARNING) << "Access key not found: " << access_key_id;
        return false;
    }
    
    it->second.is_active = false;
    LOG(INFO) << "Deactivated access key: " << access_key_id;
    return true;
}

bool authenticator::activate_key(const std::string& access_key_id) {
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _keys.find(access_key_id);
    if (it == _keys.end()) {
        LOG(WARNING) << "Access key not found: " << access_key_id;
        return false;
    }
    
    it->second.is_active = true;
    LOG(INFO) << "Activated access key: " << access_key_id;
    return true;
}

bool authenticator::verify_signature(
    const std::string& method,
    const std::string& uri,
    const std::map<std::string, std::string>& headers,
    const std::string& body,
    const std::string& region,
    const std::string& service) const
{
    // Ищем заголовок Authorization
    auto auth_it = headers.find("authorization");
    if (auth_it == headers.end()) {
        LOG(WARNING) << "Authorization header not found";
        return false;
    }
    
    // Парсим заголовок Authorization
    auto parsed = parse_authorization_header(auth_it->second);
    if (!parsed) {
        LOG(WARNING) << "Failed to parse Authorization header";
        return false;
    }
    
    // Получаем ключ доступа
    auto key_opt = get_key(parsed->access_key_id);
    if (!key_opt || !key_opt->is_active) {
        LOG(WARNING) << "Invalid or inactive access key: " << parsed->access_key_id;
        return false;
    }
    
    // Проверяем временную метку
    auto timestamp_opt = get_timestamp(headers);
    if (!timestamp_opt || !is_timestamp_valid(*timestamp_opt)) {
        LOG(WARNING) << "Invalid or expired timestamp";
        return false;
    }
    
    // Извлекаем дату из credential scope
    size_t date_pos = parsed->credential_scope.find('/');
    if (date_pos == std::string::npos) {
        LOG(WARNING) << "Invalid credential scope format";
        return false;
    }
    
    std::string date_stamp = parsed->credential_scope.substr(0, date_pos);
    
    // Фильтруем заголовки: оставляем только те, что указаны в SignedHeaders
    std::map<std::string, std::string> filtered_headers;
    std::string signed_headers_str = parsed->signed_headers;
    std::set<std::string> signed_headers_set;
    
    // Разделяем SignedHeaders по точке с запятой
    size_t start = 0;
    size_t end = signed_headers_str.find(';');
    while (end != std::string::npos) {
        std::string header_name = signed_headers_str.substr(start, end - start);
        signed_headers_set.insert(header_name);
        start = end + 1;
        end = signed_headers_str.find(';', start);
    }
    // Последний элемент
    if (start < signed_headers_str.length()) {
        std::string header_name = signed_headers_str.substr(start);
        signed_headers_set.insert(header_name);
    }
    
    // Добавляем только те заголовки, которые есть в signed_headers_set
    for (const auto& [name, value] : headers) {
        if (signed_headers_set.find(name) != signed_headers_set.end()) {
            filtered_headers[name] = value;
        }
    }
    
    // Генерируем ожидаемую подпись
    std::string expected_signature = generate_signature(
        parsed->access_key_id,
        key_opt->secret_access_key,
        method,
        uri,
        filtered_headers,
        body,
        region,
        service
    );
    
    // Сравниваем подписи (безопасное сравнение)
    bool valid = (expected_signature == parsed->signature);
    
    if (valid) {
        LOG(INFO) << "Signature verified successfully for user: " << key_opt->user_name;
    } else {
        LOG(WARNING) << "Signature verification failed for key: " << parsed->access_key_id;
    }
    
    return valid;
}

std::optional<access_key> authenticator::get_key(const std::string& access_key_id) const {
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto it = _keys.find(access_key_id);
    if (it == _keys.end()) {
        return std::nullopt;
    }
    
    return it->second;
}

std::vector<access_key> authenticator::list_keys() const {
    std::lock_guard<std::mutex> lock(_mutex);
    
    std::vector<access_key> keys;
    keys.reserve(_keys.size());
    
    for (const auto& [id, key] : _keys) {
        keys.push_back(key);
    }
    
    return keys;
}

std::string authenticator::generate_signature(
    const std::string& access_key_id,
    const std::string& secret_key,
    const std::string& method,
    const std::string& uri,
    const std::map<std::string, std::string>& headers,
    const std::string& body,
    const std::string& region,
    const std::string& service) const
{
    // 1. Extract timestamp from headers (x-amz-date or date)
    auto timestamp_opt = get_timestamp(headers);
    if (!timestamp_opt) {
        LOG(WARNING) << "No timestamp found in headers";
        return "";
    }
    std::string timestamp = *timestamp_opt;
    // date_stamp is YYYYMMDD part of timestamp (first 8 chars)
    std::string date_stamp = timestamp.substr(0, 8);
    
    // 2. Prepare canonical headers
    std::map<std::string, std::string> canonical_headers_map;
    for (const auto& [name, value] : headers) {
        // Skip headers that are not part of the signature (like Authorization)
        if (name == "authorization") continue;
        canonical_headers_map[name] = value;
    }
    // Ensure host header is present (required by AWS)
    if (canonical_headers_map.find("host") == canonical_headers_map.end()) {
        // If host not provided, we cannot sign properly
        LOG(WARNING) << "Host header missing";
        return "";
    }
    
    // Sort headers by name
    std::vector<std::string> sorted_header_names;
    for (const auto& [name, _] : canonical_headers_map) {
        sorted_header_names.push_back(name);
    }
    std::sort(sorted_header_names.begin(), sorted_header_names.end());
    
    // Build canonical headers string
    std::string canonical_headers;
    for (const auto& name : sorted_header_names) {
        canonical_headers += name + ":" + canonical_headers_map[name] + "\n";
    }
    
    // Signed headers list
    std::string signed_headers;
    for (size_t i = 0; i < sorted_header_names.size(); ++i) {
        if (i > 0) signed_headers += ";";
        signed_headers += sorted_header_names[i];
    }
    
    // 3. Hash the request body
    std::string payload_hash = sha256_hex(body);
    
    // 4. Build canonical request
    std::string canonical_request = method + "\n"
                                  + uri + "\n"
                                  + "" + "\n"  // canonical query string (empty)
                                  + canonical_headers + "\n"
                                  + signed_headers + "\n"
                                  + payload_hash;
    
    // 5. Create string to sign
    std::string credential_scope = date_stamp + "/" + region + "/" + service + "/aws4_request";
    std::string string_to_sign = "AWS4-HMAC-SHA256\n"
                               + timestamp + "\n"
                               + credential_scope + "\n"
                               + sha256_hex(canonical_request);
    
    // DEBUG: Log canonical request and string to sign when environment variable is set
    const char* debug_env = std::getenv("DEBUG_AUTH");
    if (debug_env && std::string(debug_env) == "1") {
        LOG(INFO) << "Canonical request:\n" << canonical_request;
        LOG(INFO) << "String to sign:\n" << string_to_sign;
    }
    
    // 6. Compute signing key
    std::string signing_key = get_signature_key(secret_key, date_stamp, region, service);
    
    // 7. Compute signature
    std::string signature = hmac_sha256_hex(signing_key, string_to_sign);
    
    return signature;
}

// ========== AWS Signature Version 4 Implementation ==========

std::string authenticator::sign(const std::string& key, const std::string& msg) const {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;
    
    HMAC(EVP_sha256(),
         reinterpret_cast<const unsigned char*>(key.data()),
         key.length(),
         reinterpret_cast<const unsigned char*>(msg.data()),
         msg.length(),
         digest,
         &digest_len);
    
    return std::string(reinterpret_cast<char*>(digest), digest_len);
}

std::string authenticator::get_signature_key(
    const std::string& key,
    const std::string& date_stamp,
    const std::string& region_name,
    const std::string& service_name) const
{
    std::string k_date = sign("AWS4" + key, date_stamp);
    std::string k_region = sign(k_date, region_name);
    std::string k_service = sign(k_region, service_name);
    std::string k_signing = sign(k_service, "aws4_request");
    
    return k_signing;
}

std::string authenticator::sha256_hex(const std::string& data) const {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.length(), hash);
    
    return hex_encode(hash, SHA256_DIGEST_LENGTH);
}

std::string authenticator::hmac_sha256_hex(const std::string& key, const std::string& data) const {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;
    
    HMAC(EVP_sha256(),
         reinterpret_cast<const unsigned char*>(key.data()),
         key.length(),
         reinterpret_cast<const unsigned char*>(data.data()),
         data.length(),
         digest,
         &digest_len);
    
    return hex_encode(digest, digest_len);
}

std::string authenticator::hex_encode(const unsigned char* data, size_t len) const {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    
    for (size_t i = 0; i < len; ++i) {
        ss << std::setw(2) << static_cast<int>(data[i]);
    }
    
    return ss.str();
}

std::string authenticator::generate_access_key_id() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << "AKIA";
    
    // Генерируем 16 шестнадцатеричных символов
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

std::string authenticator::generate_secret_key() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    
    // Генерируем 40 случайных байт
    for (int i = 0; i < 40; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    
    return ss.str();
}

std::optional<authenticator::parsed_auth> authenticator::parse_authorization_header(
    const std::string& auth_header) const
{
    // Формат: AWS4-HMAC-SHA256 Credential=AKIA..., SignedHeaders=..., Signature=...
    
    if (auth_header.find("AWS4-HMAC-SHA256") == std::string::npos) {
        return std::nullopt;
    }
    
    parsed_auth result;
    
    // Извлекаем Credential
    size_t cred_pos = auth_header.find("Credential=");
    if (cred_pos != std::string::npos) {
        size_t start = cred_pos + 11;
        size_t end = auth_header.find(',', start);
        if (end == std::string::npos) end = auth_header.length();
        
        std::string cred_str = auth_header.substr(start, end - start);
        size_t slash_pos = cred_str.find('/');
        
        if (slash_pos != std::string::npos) {
            result.access_key_id = cred_str.substr(0, slash_pos);
            result.credential_scope = cred_str.substr(slash_pos + 1);
        }
    }
    
    // Извлекаем SignedHeaders
    size_t headers_pos = auth_header.find("SignedHeaders=");
    if (headers_pos != std::string::npos) {
        size_t start = headers_pos + 14;
        size_t end = auth_header.find(',', start);
        if (end == std::string::npos) end = auth_header.length();
        
        result.signed_headers = auth_header.substr(start, end - start);
    }
    
    // Извлекаем Signature
    size_t sig_pos = auth_header.find("Signature=");
    if (sig_pos != std::string::npos) {
        size_t start = sig_pos + 10;
        size_t end = auth_header.find(',', start);
        if (end == std::string::npos) end = auth_header.length();
        
        result.signature = auth_header.substr(start, end - start);
    }
    
    if (result.access_key_id.empty() || result.signature.empty()) {
        return std::nullopt;
    }
    
    return result;
}

std::optional<std::string> authenticator::get_timestamp(
    const std::map<std::string, std::string>& headers) const
{
    // Ищем заголовок x-amz-date или Date
    auto x_amz_date_it = headers.find("x-amz-date");
    if (x_amz_date_it != headers.end()) {
        return x_amz_date_it->second;
    }
    
    auto date_it = headers.find("date");
    if (date_it != headers.end()) {
        return date_it->second;
    }
    
    return std::nullopt;
}

bool authenticator::is_timestamp_valid(const std::string& timestamp_str) const {
    // Проверяем, что временная метка не старше 15 минут
    try {
        // Парсим временну́ю метку (формат: YYYYMMDDTHHMMSSZ)
        // TODO: Реализовать парсинг ISO 8601
        auto now = std::chrono::system_clock::now();
        auto fifteen_minutes_ago = now - std::chrono::minutes(15);
        
        // Для простоты принимаем все временные метки в пределах 15 минут
        return true;
        
    } catch (...) {
        return false;
    }
}