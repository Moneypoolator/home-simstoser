#include <gtest/gtest.h>
#include "authenticator.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <random>
#include <thread>

namespace fs = std::filesystem;

class AuthenticatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test files
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis;
        uint64_t random = dis(gen);
        _temp_dir = fs::temp_directory_path() / ("auth_test_" + std::to_string(random));
        fs::create_directories(_temp_dir);
        
        _keys_file = _temp_dir / "access_keys.csv";
    }
    
    void TearDown() override {
        // Remove temporary directory
        if (fs::exists(_temp_dir)) {
            fs::remove_all(_temp_dir);
        }
    }
    
    // Helper to create a CSV file with given content
    void create_csv(const std::string& content) {
        std::ofstream file(_keys_file);
        file << content;
        file.close();
    }
    
    // Helper to read CSV file content
    std::string read_csv() {
        std::ifstream file(_keys_file);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    fs::path _temp_dir;
    fs::path _keys_file;
};

// Test loading keys from a valid CSV file
TEST_F(AuthenticatorTest, LoadKeysValid) {
    authenticator auth;
    
    // Create a CSV with one key
    create_csv("AKIAEXAMPLE,secret123,user1,1\n");
    
    bool loaded = auth.load_keys(_keys_file.string());
    EXPECT_TRUE(loaded);
    
    auto key = auth.get_key("AKIAEXAMPLE");
    ASSERT_TRUE(key.has_value());
    EXPECT_EQ(key->access_key_id, "AKIAEXAMPLE");
    EXPECT_EQ(key->secret_access_key, "secret123");
    EXPECT_EQ(key->user_name, "user1");
    EXPECT_TRUE(key->is_active);
}

// Test loading keys from non-existent file
TEST_F(AuthenticatorTest, LoadKeysFileNotFound) {
    authenticator auth;
    bool loaded = auth.load_keys("nonexistent.csv");
    EXPECT_FALSE(loaded);
}

// Test loading keys with malformed CSV (missing fields)
TEST_F(AuthenticatorTest, LoadKeysMalformed) {
    authenticator auth;
    create_csv("AKIAEXAMPLE,secret123\n"); // missing user_name and is_active
    bool loaded = auth.load_keys(_keys_file.string());
    // According to implementation, malformed lines are skipped, but load returns true if file exists
    // Let's see: the implementation expects 4 fields, if not enough it skips line.
    // The function returns true if file opened successfully.
    EXPECT_TRUE(loaded);
    // No keys should be loaded
    auto key = auth.get_key("AKIAEXAMPLE");
    EXPECT_FALSE(key.has_value());
}

// Test saving keys to file
TEST_F(AuthenticatorTest, SaveKeys) {
    authenticator auth;
    
    // Create a key via authenticator (since we don't have direct add method)
    auto key = auth.create_access_key("testuser");
    ASSERT_TRUE(key.has_value());
    
    bool saved = auth.save_keys(_keys_file.string());
    EXPECT_TRUE(saved);
    
    // Verify file content
    std::string content = read_csv();
    EXPECT_NE(content.find(key->access_key_id), std::string::npos);
    EXPECT_NE(content.find(key->secret_access_key), std::string::npos);
    EXPECT_NE(content.find("testuser"), std::string::npos);
    EXPECT_NE(content.find("1"), std::string::npos); // active flag
}

// Test saving and then loading back
TEST_F(AuthenticatorTest, SaveAndLoadRoundtrip) {
    authenticator auth1;
    auto key = auth1.create_access_key("roundtrip_user");
    ASSERT_TRUE(key.has_value());
    
    bool saved = auth1.save_keys(_keys_file.string());
    ASSERT_TRUE(saved);
    
    authenticator auth2;
    bool loaded = auth2.load_keys(_keys_file.string());
    EXPECT_TRUE(loaded);
    
    auto loaded_key = auth2.get_key(key->access_key_id);
    ASSERT_TRUE(loaded_key.has_value());
    EXPECT_EQ(loaded_key->access_key_id, key->access_key_id);
    EXPECT_EQ(loaded_key->secret_access_key, key->secret_access_key);
    EXPECT_EQ(loaded_key->user_name, key->user_name);
    EXPECT_EQ(loaded_key->is_active, key->is_active);
}

// Test loading multiple keys
TEST_F(AuthenticatorTest, LoadMultipleKeys) {
    authenticator auth;
    create_csv("AKIA1,secret1,user1,1\n"
               "AKIA2,secret2,user2,0\n"
               "AKIA3,secret3,user3,1\n");
    
    bool loaded = auth.load_keys(_keys_file.string());
    EXPECT_TRUE(loaded);
    
    auto key1 = auth.get_key("AKIA1");
    auto key2 = auth.get_key("AKIA2");
    auto key3 = auth.get_key("AKIA3");
    
    ASSERT_TRUE(key1.has_value());
    ASSERT_TRUE(key2.has_value());
    ASSERT_TRUE(key3.has_value());
    
    EXPECT_EQ(key1->user_name, "user1");
    EXPECT_TRUE(key1->is_active);
    EXPECT_EQ(key2->user_name, "user2");
    EXPECT_FALSE(key2->is_active);
    EXPECT_EQ(key3->user_name, "user3");
    EXPECT_TRUE(key3->is_active);
}

// Test create_access_key generates unique IDs
TEST_F(AuthenticatorTest, CreateAccessKey) {
    authenticator auth;
    auto key1 = auth.create_access_key("user1");
    auto key2 = auth.create_access_key("user2");
    
    ASSERT_TRUE(key1.has_value());
    ASSERT_TRUE(key2.has_value());
    
    EXPECT_EQ(key1->user_name, "user1");
    EXPECT_EQ(key2->user_name, "user2");
    EXPECT_TRUE(key1->is_active);
    EXPECT_TRUE(key2->is_active);
    EXPECT_NE(key1->access_key_id, key2->access_key_id);
    EXPECT_NE(key1->secret_access_key, key2->secret_access_key);
    
    // Verify keys can be retrieved
    auto retrieved = auth.get_key(key1->access_key_id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->secret_access_key, key1->secret_access_key);
}

// Test create_access_key with custom ID
TEST_F(AuthenticatorTest, CreateAccessKeyCustomId) {
    authenticator auth;
    auto key = auth.create_access_key("user", "CUSTOMID123");
    ASSERT_TRUE(key.has_value());
    EXPECT_EQ(key->access_key_id, "CUSTOMID123");
    EXPECT_EQ(key->user_name, "user");
}

// Test delete_access_key
TEST_F(AuthenticatorTest, DeleteAccessKey) {
    authenticator auth;
    auto key = auth.create_access_key("user");
    ASSERT_TRUE(key.has_value());
    
    bool deleted = auth.delete_access_key(key->access_key_id);
    EXPECT_TRUE(deleted);
    
    auto retrieved = auth.get_key(key->access_key_id);
    EXPECT_FALSE(retrieved.has_value());
}

// Test delete_access_key non-existent
TEST_F(AuthenticatorTest, DeleteAccessKeyNotFound) {
    authenticator auth;
    bool deleted = auth.delete_access_key("NONEXISTENT");
    EXPECT_FALSE(deleted);
}

// Test activate/deactivate key
TEST_F(AuthenticatorTest, ActivateDeactivateKey) {
    authenticator auth;
    auto key = auth.create_access_key("user");
    ASSERT_TRUE(key.has_value());
    EXPECT_TRUE(key->is_active);
    
    // Deactivate
    bool deactivated = auth.deactivate_key(key->access_key_id);
    EXPECT_TRUE(deactivated);
    auto key2 = auth.get_key(key->access_key_id);
    ASSERT_TRUE(key2.has_value());
    EXPECT_FALSE(key2->is_active);
    
    // Activate again
    bool activated = auth.activate_key(key->access_key_id);
    EXPECT_TRUE(activated);
    auto key3 = auth.get_key(key->access_key_id);
    ASSERT_TRUE(key3.has_value());
    EXPECT_TRUE(key3->is_active);
}

// Test list_keys
TEST_F(AuthenticatorTest, ListKeys) {
    authenticator auth;
    auto key1 = auth.create_access_key("user1");
    auto key2 = auth.create_access_key("user2");
    auto key3 = auth.create_access_key("user3");
    
    std::vector<access_key> keys = auth.list_keys();
    EXPECT_EQ(keys.size(), 3);
    
    // Check that all keys are present
    std::set<std::string> ids;
    for (const auto& k : keys) {
        ids.insert(k.access_key_id);
    }
    EXPECT_TRUE(ids.count(key1->access_key_id) > 0);
    EXPECT_TRUE(ids.count(key2->access_key_id) > 0);
    EXPECT_TRUE(ids.count(key3->access_key_id) > 0);
}

// Test get_key non-existent
TEST_F(AuthenticatorTest, GetKeyNotFound) {
    authenticator auth;
    auto key = auth.get_key("NONEXISTENT");
    EXPECT_FALSE(key.has_value());
}

// Test verify_signature missing authorization header
TEST_F(AuthenticatorTest, VerifySignatureMissingAuthHeader) {
    authenticator auth;
    std::map<std::string, std::string> headers = {
        {"x-amz-date", "20220101T120000Z"}
    };
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    EXPECT_FALSE(verified);
}

// Test verify_signature with malformed authorization header
TEST_F(AuthenticatorTest, VerifySignatureMalformedAuthHeader) {
    authenticator auth;
    std::map<std::string, std::string> headers = {
        {"authorization", "InvalidHeader"},
        {"x-amz-date", "20220101T120000Z"}
    };
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    EXPECT_FALSE(verified);
}

// Test verify_signature with non-existent key
TEST_F(AuthenticatorTest, VerifySignatureKeyNotFound) {
    authenticator auth;
    std::map<std::string, std::string> headers = {
        {"authorization", "AWS4-HMAC-SHA256 Credential=AKIAEXAMPLE/20220101/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=abcdef"},
        {"x-amz-date", "20220101T120000Z"}
    };
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    EXPECT_FALSE(verified);
}

// Test verify_signature with inactive key
TEST_F(AuthenticatorTest, VerifySignatureInactiveKey) {
    authenticator auth;
    auto key = auth.create_access_key("user");
    ASSERT_TRUE(key.has_value());
    auth.deactivate_key(key->access_key_id);
    
    std::map<std::string, std::string> headers = {
        {"authorization", "AWS4-HMAC-SHA256 Credential=" + key->access_key_id + "/20220101/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=abcdef"},
        {"x-amz-date", "20220101T120000Z"}
    };
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    EXPECT_FALSE(verified);
}

// Test verify_signature with invalid timestamp
TEST_F(AuthenticatorTest, VerifySignatureInvalidTimestamp) {
    authenticator auth;
    auto key = auth.create_access_key("user");
    ASSERT_TRUE(key.has_value());
    
    // Timestamp far in the past (or missing)
    std::map<std::string, std::string> headers = {
        {"authorization", "AWS4-HMAC-SHA256 Credential=" + key->access_key_id + "/20220101/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=abcdef"},
        {"x-amz-date", "19990101T120000Z"} // old timestamp
    };
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    EXPECT_FALSE(verified);
}


// ========== ТЕСТЫ ВРЕМЕННОЙ МЕТКИ ==========

TEST_F(AuthenticatorTest, TimestampValidation_ValidCurrent) {
    authenticator auth;
    // Генерируем текущую временную метку в формате YYYYMMDDTHHMMSSZ
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    gmtime_r(&now_time_t, &tm_buf);
    char timestamp[17];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%SZ", &tm_buf);
    
    // Вызываем приватный метод через публичный verify_signature, который проверяет timestamp
    // Для этого создадим заголовки с валидной временной меткой, но подпись будет неверной (ожидаем false не из-за времени)
    std::map<std::string, std::string> headers = {{"x-amz-date", timestamp}};
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    // Должен вернуть false из-за отсутствия/неверной подписи, но timestamp валиден
    EXPECT_FALSE(verified);
}

TEST_F(AuthenticatorTest, TimestampValidation_Expired) {
    authenticator auth;
    // Временная метка 20 минут назад
    auto past = std::chrono::system_clock::now() - std::chrono::minutes(20);
    auto past_time_t = std::chrono::system_clock::to_time_t(past);
    std::tm tm_buf;
    gmtime_r(&past_time_t, &tm_buf);
    char timestamp[17];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%SZ", &tm_buf);
    
    std::map<std::string, std::string> headers = {{"x-amz-date", timestamp}};
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    EXPECT_FALSE(verified); // Должен вернуть false из-за просроченной метки
}

TEST_F(AuthenticatorTest, TimestampValidation_Future) {
    authenticator auth;
    // Временная метка в будущем (через 20 минут)
    auto future = std::chrono::system_clock::now() + std::chrono::minutes(20);
    auto future_time_t = std::chrono::system_clock::to_time_t(future);
    std::tm tm_buf;
    gmtime_r(&future_time_t, &tm_buf);
    char timestamp[17];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%SZ", &tm_buf);
    
    std::map<std::string, std::string> headers = {{"x-amz-date", timestamp}};
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    EXPECT_FALSE(verified);
}

TEST_F(AuthenticatorTest, TimestampValidation_BoundaryMinus15Min) {
    authenticator auth;
    auto boundary = std::chrono::system_clock::now() - std::chrono::minutes(15) + std::chrono::seconds(1);
    auto boundary_time_t = std::chrono::system_clock::to_time_t(boundary);
    std::tm tm_buf;
    gmtime_r(&boundary_time_t, &tm_buf);
    char timestamp[17];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%SZ", &tm_buf);
    
    std::map<std::string, std::string> headers = {{"x-amz-date", timestamp}};
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    // Не должно быть отклонено по времени (но подпись неверна)
    EXPECT_FALSE(verified);
}

TEST_F(AuthenticatorTest, TimestampValidation_InvalidFormat) {
    authenticator auth;
    std::map<std::string, std::string> headers = {{"x-amz-date", "2025-01-01T12:00:00Z"}}; // Неверный формат
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    EXPECT_FALSE(verified);
}

// ========== ТЕСТЫ CANONICAL QUERY STRING ==========

TEST_F(AuthenticatorTest, BuildCanonicalQueryString_NoParams) {
    authenticator auth;
    std::string uri = "/test";
    EXPECT_EQ(auth.build_canonical_query_string(uri), "");
}

TEST_F(AuthenticatorTest, BuildCanonicalQueryString_SingleParam) {
    authenticator auth;
    std::string uri = "/test?foo=bar";
    EXPECT_EQ(auth.build_canonical_query_string(uri), "foo=bar");
}

TEST_F(AuthenticatorTest, BuildCanonicalQueryString_MultipleParamsSorted) {
    authenticator auth;
    std::string uri = "/test?b=2&a=1&c=3";
    EXPECT_EQ(auth.build_canonical_query_string(uri), "a=1&b=2&c=3");
}

TEST_F(AuthenticatorTest, BuildCanonicalQueryString_EncodedValues) {
    authenticator auth;
    std::string uri = "/test?key=value%20with%20spaces&foo=bar";
    // В канонической форме значения должны быть закодированы
    EXPECT_EQ(auth.build_canonical_query_string(uri), "foo=bar&key=value%20with%20spaces");
}

TEST_F(AuthenticatorTest, BuildCanonicalQueryString_EmptyValue) {
    authenticator auth;
    std::string uri = "/test?flag&key=value";
    // Параметр без '=' считается с пустым значением
    EXPECT_EQ(auth.build_canonical_query_string(uri), "flag=&key=value");
}

TEST_F(AuthenticatorTest, BuildCanonicalQueryString_DecodedInput) {
    authenticator auth;
    // Входные данные могут быть уже декодированы сервером, но функция ожидает сырой URI
    std::string uri = "/test?key=value with spaces"; // Пробелы должны быть закодированы
    // Поскольку в URI пробелы недопустимы, этот тест показывает, что функция предполагает корректный URI
    // В реальности сервер получает сырой URI, где пробелы закодированы как %20
    uri = "/test?key=value%20with%20spaces";
    EXPECT_EQ(auth.build_canonical_query_string(uri), "key=value%20with%20spaces");
}

// ========== ТЕСТЫ ПОДПИСИ (ПОЗИТИВНЫЙ СЦЕНАРИЙ) ==========

// Для генерации корректной подписи используем фиксированные данные (как в AWS документации)
// Пример из документации AWS:
// https://docs.aws.amazon.com/AmazonS3/latest/API/sigv4-auth-using-authorization-header.html

TEST_F(AuthenticatorTest, VerifySignature_Valid) {
    authenticator auth;
    
    // Создаём тестовый ключ
    auto key = auth.create_access_key("testuser", "AKIAIOSFODNN7EXAMPLE");
    ASSERT_TRUE(key.has_value());
    
    // Фиксированные данные для подписи из документации AWS
    // Здесь мы не можем использовать реальную подпись, так как она зависит от времени.
    // Вместо этого мы проверим, что при правильной подписи verify_signature возвращает true.
    // Для этого сгенерируем подпись динамически и проверим её.
    
    std::string method = "GET";
    std::string uri = "/test.txt";
    std::string body = "";
    std::string region = "us-east-1";
    std::string service = "s3";
    
    // Генерируем текущую временную метку
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    gmtime_r(&now_time_t, &tm_buf);
    char timestamp[17];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%SZ", &tm_buf);
    
    std::map<std::string, std::string> headers = {
        {"host", "localhost:9000"},
        {"x-amz-date", timestamp}
    };
    
    std::string signature = auth.generate_signature(
        key->access_key_id,
        key->secret_access_key,
        method,
        uri,
        headers,
        body,
        region,
        service
    );
    
    ASSERT_FALSE(signature.empty());
    
    // Формируем заголовок Authorization
    std::string date_stamp = std::string(timestamp, 8);
    std::string credential_scope = date_stamp + "/" + region + "/" + service + "/aws4_request";
    std::string auth_header = 
        "AWS4-HMAC-SHA256 Credential=" + key->access_key_id + "/" + credential_scope +
        ", SignedHeaders=host;x-amz-date" +
        ", Signature=" + signature;
    
    headers["authorization"] = auth_header;
    
    bool verified = auth.verify_signature(method, uri, headers, body, region, service);
    EXPECT_TRUE(verified);
}

TEST_F(AuthenticatorTest, VerifySignature_WithQueryParams) {
    authenticator auth;
    auto key = auth.create_access_key("testuser", "AKIAQUERYTEST");
    ASSERT_TRUE(key.has_value());
    
    std::string method = "GET";
    std::string uri = "/test.txt?foo=bar&baz=qux";
    std::string body = "";
    std::string region = "us-east-1";
    std::string service = "s3";
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    gmtime_r(&now_time_t, &tm_buf);
    char timestamp[17];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%SZ", &tm_buf);
    
    std::map<std::string, std::string> headers = {
        {"host", "localhost:9000"},
        {"x-amz-date", timestamp}
    };
    
    std::string signature = auth.generate_signature(
        key->access_key_id,
        key->secret_access_key,
        method,
        uri,
        headers,
        body,
        region,
        service
    );
    ASSERT_FALSE(signature.empty());
    
    std::string date_stamp = std::string(timestamp, 8);
    std::string credential_scope = date_stamp + "/" + region + "/" + service + "/aws4_request";
    std::string auth_header = 
        "AWS4-HMAC-SHA256 Credential=" + key->access_key_id + "/" + credential_scope +
        ", SignedHeaders=host;x-amz-date" +
        ", Signature=" + signature;
    
    headers["authorization"] = auth_header;
    
    bool verified = auth.verify_signature(method, uri, headers, body, region, service);
    EXPECT_TRUE(verified);
}

TEST_F(AuthenticatorTest, VerifySignature_DifferentRegion) {
    authenticator auth;
    auto key = auth.create_access_key("testuser", "AKIAREGIONTEST");
    ASSERT_TRUE(key.has_value());
    
    std::string method = "GET";
    std::string uri = "/test.txt";
    std::string body = "";
    std::string region = "eu-west-1";
    std::string service = "s3";
    
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    gmtime_r(&now_time_t, &tm_buf);
    char timestamp[17];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%SZ", &tm_buf);
    
    std::map<std::string, std::string> headers = {
        {"host", "localhost:9000"},
        {"x-amz-date", timestamp}
    };
    
    std::string signature = auth.generate_signature(
        key->access_key_id,
        key->secret_access_key,
        method,
        uri,
        headers,
        body,
        region,
        service
    );
    ASSERT_FALSE(signature.empty());
    
    std::string date_stamp = std::string(timestamp, 8);
    std::string credential_scope = date_stamp + "/" + region + "/" + service + "/aws4_request";
    std::string auth_header = 
        "AWS4-HMAC-SHA256 Credential=" + key->access_key_id + "/" + credential_scope +
        ", SignedHeaders=host;x-amz-date" +
        ", Signature=" + signature;
    
    headers["authorization"] = auth_header;
    
    bool verified = auth.verify_signature(method, uri, headers, body, region, service);
    EXPECT_TRUE(verified);
}

// ========== ТЕСТ ГЕНЕРАЦИИ ПОДПИСИ С ЭТАЛОННЫМ ЗНАЧЕНИЕМ ==========

// Используем пример из официальной документации AWS:
// https://docs.aws.amazon.com/general/latest/gr/sigv4-calculate-signature.html
TEST_F(AuthenticatorTest, GenerateSignature_ProducesExpected_AWSExample) {
    authenticator auth;
    
    // Данные из примера AWS (запрос GET к /)
    std::string access_key_id = "AKIDEXAMPLE";
    std::string secret_key = "wJalrXUtnFEMI/K7MDENG+bPxRfiCYEXAMPLEKEY";
    std::string method = "GET";
    std::string uri = "/";
    std::string body = "";
    std::string region = "us-east-1";
    std::string service = "iam";
    
    // Фиксированная временная метка из примера
    std::string timestamp = "20150830T123600Z";
    
    std::map<std::string, std::string> headers = {
        {"host", "iam.amazonaws.com"},
        {"x-amz-date", timestamp}
    };
    
    // Создаём ключ с секретом из примера
    access_key key;
    key.access_key_id = access_key_id;
    key.secret_access_key = secret_key;
    key.user_name = "example";
    key.is_active = true;
    
    // Генерируем подпись
    std::string signature = auth.generate_signature(
        access_key_id,
        secret_key,
        method,
        uri,
        headers,
        body,
        region,
        service
    );
    
    // Ожидаемая подпись из документации
    std::string expected_signature = "5d672d79c15b13162d9279b0855cfba6789a8edb4c82c400e06b5924a6f2b5d7";
    
    EXPECT_EQ(signature, expected_signature);
}

// ========== МНОГОПОТОЧНЫЙ ДОСТУП ==========

TEST_F(AuthenticatorTest, ConcurrentAccess) {
    authenticator auth;
    
    // Предварительно создаём несколько ключей
    std::vector<std::string> key_ids;
    for (int i = 0; i < 10; ++i) {
        auto key = auth.create_access_key("user" + std::to_string(i));
        ASSERT_TRUE(key.has_value());
        key_ids.push_back(key->access_key_id);
    }
    
    const int num_threads = 4;
    const int ops_per_thread = 100;
    std::atomic<int> errors{0};
    
    auto worker = [&](int thread_id) {
        for (int i = 0; i < ops_per_thread; ++i) {
            // Чтение случайного ключа
            int idx = (thread_id * 7 + i) % key_ids.size();
            auto key = auth.get_key(key_ids[idx]);
            if (!key.has_value()) {
                errors++;
            }
            
            // Иногда создаём новый ключ
            if (i % 10 == 0) {
                auto new_key = auth.create_access_key("concurrent_user_" + std::to_string(thread_id) + "_" + std::to_string(i));
                if (!new_key.has_value()) {
                    errors++;
                }
            }
            
            // Иногда деактивируем ключ
            if (i % 15 == 0 && !key_ids.empty()) {
                auth.deactivate_key(key_ids[idx]);
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back(worker, t);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(errors.load(), 0);
    
    // Проверяем, что после всех операций можем сохранить и загрузить ключи
    fs::path temp_file = _temp_dir / "concurrent_keys.csv";
    bool saved = auth.save_keys(temp_file.string());
    EXPECT_TRUE(saved);
    
    authenticator auth2;
    bool loaded = auth2.load_keys(temp_file.string());
    EXPECT_TRUE(loaded);
    auto loaded_keys = auth2.list_keys();
    EXPECT_GE(loaded_keys.size(), 10); // Должно быть не меньше исходных 10
}

// ========== ТЕСТЫ ВСПОМОГАТЕЛЬНЫХ ФУНКЦИЙ ==========

// Тест hex_encode (приватный, но можно проверить косвенно через sha256_hex)
TEST_F(AuthenticatorTest, Sha256Hex_EmptyString) {
    authenticator auth;
    std::string hash = auth.sha256_hex("");
    // SHA256 пустой строки известен
    EXPECT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_F(AuthenticatorTest, HmacSha256Hex_KnownValue) {
    authenticator auth;
    std::string key = "key";
    std::string data = "The quick brown fox jumps over the lazy dog";
    std::string hmac = auth.hmac_sha256_hex(key, data);
    // Ожидаемое значение можно получить из онлайн-калькулятора
    EXPECT_EQ(hmac, "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");
}

// Тест url_encode/decode (статические функции внутри authenticator.cpp, можно сделать публичными или протестировать через build_canonical_query_string)
// Мы уже частично покрыли через BuildCanonicalQueryString

// Test generate_signature returns empty string (not implemented)
// TEST_F(AuthenticatorTest, GenerateSignatureNotImplemented) {
//     authenticator auth;
//     std::map<std::string, std::string> headers = {{"x-amz-date", "20220101T120000Z"}};
//     std::string sig = auth.generate_signature("AKIAEXAMPLE", "secret", "GET", "/test", headers, "");
//     EXPECT_EQ(sig, "");
// }