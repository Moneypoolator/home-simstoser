#pragma once

#include <mutex>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <chrono>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

namespace fs = std::filesystem;

struct access_key {
    std::string access_key_id;
    std::string secret_access_key;
    std::string user_name;
    bool is_active = true;
    std::chrono::system_clock::time_point created_at;
};

class authenticator {
public:
    authenticator();
    ~authenticator() = default;
    
    // Загрузить ключи доступа из файла
    [[nodiscard]] bool load_keys(const std::string& filepath);
    
    // Сохранить ключи доступа в файл
    [[nodiscard]] bool save_keys(const std::string& filepath) const;
    
    // Создать новый ключ доступа
    std::optional<access_key> create_access_key(
        const std::string& user_name,
        const std::string& access_key_id = ""
    );
    
    // Удалить ключ доступа
    [[nodiscard]] bool delete_access_key(const std::string& access_key_id);
    
    // Деактивировать ключ доступа
    [[nodiscard]] bool deactivate_key(const std::string& access_key_id);
    
    // Активировать ключ доступа
    [[nodiscard]] bool activate_key(const std::string& access_key_id);
    
    // Проверить подпись запроса (AWS Signature Version 4)
    [[nodiscard]] bool verify_signature(
        const std::string& method,
        const std::string& uri,
        const std::map<std::string, std::string>& headers,
        const std::string& body,
        const std::string& region = "us-east-1",
        const std::string& service = "s3"
    ) const;
    
    // Получить ключ доступа по ID
    std::optional<access_key> get_key(const std::string& access_key_id) const;
    
    // Список всех ключей доступа
    std::vector<access_key> list_keys() const;
    
    // Генерация подписи для клиента (для тестирования)
    std::string generate_signature(
        const std::string& access_key_id,
        const std::string& secret_key,
        const std::string& method,
        const std::string& uri,
        const std::map<std::string, std::string>& headers,
        const std::string& body,
        const std::string& region = "us-east-1",
        const std::string& service = "s3"
    ) const;

    // Построение канонической строки запроса (отсортированные и закодированные параметры)
    std::string build_canonical_query_string(const std::string& uri) const;
    

    private:
        std::map<std::string, access_key> _keys;
        mutable std::mutex _mutex;

        // AWS Signature Version 4 методы
        std::string sign(const std::string& key, const std::string& msg) const;
        std::string get_signature_key(
            const std::string& key,
            const std::string& date_stamp,
            const std::string& region_name,
            const std::string& service_name) const;
        std::string sha256_hex(const std::string& data) const;
        std::string hmac_sha256_hex(const std::string& key, const std::string& data) const;
        std::string hex_encode(const unsigned char* data, size_t len) const;

        // Генерация уникального access_key_id
        std::string generate_access_key_id() const;

        // Генерация секретного ключа
        std::string generate_secret_key() const;

        // Парсинг Authorization header
        struct parsed_auth {
            std::string access_key_id;
            std::string signed_headers;
            std::string signature;
            std::string credential_scope;
            std::map<std::string, std::string> query_params;
        };

        std::optional<parsed_auth> parse_authorization_header(const std::string& auth_header) const;

        // Извлечение временной метки из заголовков
        std::optional<std::string> get_timestamp(const std::map<std::string, std::string>& headers) const;

        // Проверка временной метки (защита от replay атак)
        [[nodiscard]] bool is_timestamp_valid(const std::string& timestamp_str) const;
    };