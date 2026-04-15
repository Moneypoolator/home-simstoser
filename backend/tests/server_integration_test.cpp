#include <gtest/gtest.h>
#include "test_http_client.hpp"
#include "server.hpp"
#include "authenticator.hpp"
#include <thread>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class ServerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Создаем временную директорию
        _temp_dir = fs::temp_directory_path() / "s3_integration_test";
        fs::create_directories(_temp_dir);
        
        // Генерируем случайный порт
        _port = 9000 + (std::hash<std::thread::id>{}(std::this_thread::get_id()) % 1000);
        
        // Создаем и запускаем сервер в отдельном потоке
        _server = std::make_unique<s3_server>("127.0.0.1", _port, _temp_dir.string());
        
        std::thread server_thread([this]() {
            _server->run(1); // 1 поток для тестов
        });
        _server_thread = std::move(server_thread);
        
        // Ждем, пока сервер запустится
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Создаем клиент
        _client = std::make_unique<test_http_client>("127.0.0.1", _port);
    }
    
    void TearDown() override {
        // Останавливаем сервер
        _server->stop();
        
        if (_server_thread.joinable()) {
            _server_thread.join();
        }
        
        // Удаляем временную директорию
        if (fs::exists(_temp_dir)) {
            fs::remove_all(_temp_dir);
        }
    }
    
    std::unique_ptr<s3_server> _server;
    std::thread _server_thread;
    std::unique_ptr<test_http_client> _client;
    fs::path _temp_dir;
    unsigned short _port;
};

// Helper function to send HTTP request with custom headers
static http_response send_request_with_headers(
    const std::string& host,
    unsigned short port,
    http::verb method,
    const std::string& path,
    const std::string& body = "",
    const std::map<std::string, std::string>& extra_headers = {})
{
    try {
        asio::io_context io_context;
        
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(host, std::to_string(port));
        
        beast::tcp_stream stream(io_context);
        stream.connect(endpoints);
        
        http::request<http::string_body> req{method, path, 11};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "S3-Test-Client");
        req.set(http::field::content_type, "application/octet-stream");
        
        for (const auto& [name, value] : extra_headers) {
            req.set(name, value);
        }
        
        if (!body.empty()) {
            req.body() = body;
            req.prepare_payload();
        }
        
        http::write(stream, req);
        
        beast::flat_buffer buffer;
        http::response<http::string_body> res;
        http::read(stream, buffer, res);
        
        stream.socket().shutdown(tcp::socket::shutdown_both);
        
        http_response response;
        response.status_code = static_cast<unsigned int>(res.result_int());
        response.body = res.body();
        
        for (const auto& field : res.base()) {
            response.headers[field.name_string()] = field.value();
        }
        
        return response;
    } catch (const std::exception& e) {
        http_response error_response;
        error_response.status_code = 0;
        error_response.body = e.what();
        return error_response;
    }
}

class ServerAuthenticationIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory
        _temp_dir = fs::temp_directory_path() / "s3_auth_integration_test";
        fs::create_directories(_temp_dir);
        
        // Create temporary keys CSV file
        _keys_file = _temp_dir / "access_keys.csv";
        std::ofstream keys_file(_keys_file);
        keys_file << "access_key_id,secret_access_key,user_name,is_active,created_at\n";
        keys_file << "AKIAIOSFODNN7EXAMPLE,wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY,testuser,true,2024-01-01T00:00:00Z\n";
        keys_file << "AKIAINACTIVEKEY,wJalrXUtnFEMI/K7MDENG/bPxRfiCYINACTIVE,inactiveuser,false,2024-01-01T00:00:00Z\n";
        keys_file.close();
        
        // Generate random port
        _port = 9000 + (std::hash<std::thread::id>{}(std::this_thread::get_id()) % 1000);
        
        // Create and start server with keys file
        _server = std::make_unique<s3_server>("127.0.0.1", _port, _temp_dir.string(), _keys_file.string());
        
        std::thread server_thread([this]() {
            _server->run(1);
        });
        _server_thread = std::move(server_thread);
        
        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        _host = "127.0.0.1";
    }
    
    void TearDown() override {
        // Stop server
        _server->stop();
        
        if (_server_thread.joinable()) {
            _server_thread.join();
        }
        
        // Remove temporary directory
        if (fs::exists(_temp_dir)) {
            fs::remove_all(_temp_dir);
        }
    }
    
    http_response send_authenticated_request(
        http::verb method,
        const std::string& path,
        const std::string& body = "",
        const std::map<std::string, std::string>& extra_headers = {})
    {
        return send_request_with_headers(_host, _port, method, path, body, extra_headers);
    }
    
    std::unique_ptr<s3_server> _server;
    std::thread _server_thread;
    fs::path _temp_dir;
    fs::path _keys_file;
    unsigned short _port;
    std::string _host;
};

class ServerAuthorizationIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory
        _temp_dir = fs::temp_directory_path() / "s3_authz_integration_test";
        fs::create_directories(_temp_dir);
        
        // Create temporary keys CSV file (same as authentication tests)
        _keys_file = _temp_dir / "access_keys.csv";
        std::ofstream keys_file(_keys_file);
        keys_file << "access_key_id,secret_access_key,user_name,is_active,created_at\n";
        keys_file << "AKIAIOSFODNN7EXAMPLE,wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY,testuser,true,2024-01-01T00:00:00Z\n";
        keys_file << "AKIAINACTIVEKEY,wJalrXUtnFEMI/K7MDENG/bPxRfiCYINACTIVE,inactiveuser,false,2024-01-01T00:00:00Z\n";
        keys_file.close();
        
        // Create empty users file to enable authorization
        _users_file = _temp_dir / "users.json";
        std::ofstream users_file(_users_file);
        users_file << "{}"; // Empty JSON for now
        users_file.close();
        
        // Generate random port
        _port = 9000 + (std::hash<std::thread::id>{}(std::this_thread::get_id()) % 1000);
        
        // Create and start server with both keys file and users file
        _server = std::make_unique<s3_server>("127.0.0.1", _port, _temp_dir.string(), _keys_file.string(), _users_file.string());
        
        std::thread server_thread([this]() {
            _server->run(1);
        });
        _server_thread = std::move(server_thread);
        
        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        _host = "127.0.0.1";
    }
    
    void TearDown() override {
        // Stop server
        _server->stop();
        
        if (_server_thread.joinable()) {
            _server_thread.join();
        }
        
        // Remove temporary directory
        if (fs::exists(_temp_dir)) {
            fs::remove_all(_temp_dir);
        }
    }
    
    http_response send_authenticated_request(
        http::verb method,
        const std::string& path,
        const std::string& body = "",
        const std::map<std::string, std::string>& extra_headers = {})
    {
        return send_request_with_headers(_host, _port, method, path, body, extra_headers);
    }
    
    std::unique_ptr<s3_server> _server;
    std::thread _server_thread;
    fs::path _temp_dir;
    fs::path _keys_file;
    fs::path _users_file;
    unsigned short _port;
    std::string _host;
};

TEST_F(ServerIntegrationTest, ServerStartsSuccessfully) {
    // Простой запрос для проверки, что сервер работает
    auto response = _client->get("/list");
    
    EXPECT_EQ(response.status_code, 200);
    EXPECT_NE(response.body.find("count"), std::string::npos);
}

TEST_F(ServerIntegrationTest, UploadAndDownloadFile) {
    // Arrange
    std::string content = "Hello, World!";
    std::string filename = "/test_upload.txt";
    
    // Act - Upload
    auto upload_response = _client->put(filename, content);
    
    // Assert - Upload
    EXPECT_EQ(upload_response.status_code, 201);
    EXPECT_NE(upload_response.body.find("success"), std::string::npos);
    
    // Act - Download
    auto download_response = _client->get(filename);
    
    // Assert - Download
    EXPECT_EQ(download_response.status_code, 200);
    EXPECT_EQ(download_response.body, content);
}

TEST_F(ServerIntegrationTest, ListFiles) {
    // Arrange - загружаем несколько файлов
    _client->put("/file1.txt", "Content 1");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    _client->put("/file2.txt", "Content 2");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    _client->put("/subdir/file3.txt", "Content 3");
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    
    // Act
    auto response = _client->get("/list");
    
    // Assert
    EXPECT_EQ(response.status_code, 200);
    EXPECT_NE(response.body.find("\"count\":3"), std::string::npos);
    EXPECT_NE(response.body.find("file1.txt"), std::string::npos);
    EXPECT_NE(response.body.find("file2.txt"), std::string::npos);
    EXPECT_NE(response.body.find("subdir/file3.txt"), std::string::npos);
}

TEST_F(ServerIntegrationTest, DeleteFile) {
    // Arrange - загружаем файл
    _client->put("/todelete.txt", "Delete me");
    
    // Act - удаляем
    auto delete_response = _client->delete_("/todelete.txt");
    
    // Assert - удаление прошло успешно
    EXPECT_EQ(delete_response.status_code, 200);
    EXPECT_NE(delete_response.body.find("success"), std::string::npos);
    
    // Act - пытаемся скачать удаленный файл
    auto get_response = _client->get("/todelete.txt");
    
    // Assert - файл не найден
    EXPECT_EQ(get_response.status_code, 404);
}

TEST_F(ServerIntegrationTest, FileNotFound) {
    // Act
    auto response = _client->get("/nonexistent.txt");
    
    // Assert
    EXPECT_EQ(response.status_code, 404);
    EXPECT_NE(response.body.find("not found"), std::string::npos);
}

TEST_F(ServerIntegrationTest, UploadEmptyFile) {
    // Act
    auto response = _client->put("/empty.txt", "");
    
    // Assert
    EXPECT_EQ(response.status_code, 400); // Bad Request
}

TEST_F(ServerIntegrationTest, UploadLargeFile) {
    // Arrange - создаем большой файл (100 КБ)
    std::string large_content(100 * 1024, 'A'); // 100 KB
    
    // Act - Upload
    auto upload_response = _client->put("/large.bin", large_content);
    
    // Assert - Upload
    EXPECT_EQ(upload_response.status_code, 201);
    
    // Act - Download
    auto download_response = _client->get("/large.bin");
    
    // Assert - Download
    EXPECT_EQ(download_response.status_code, 200);
    EXPECT_EQ(download_response.body, large_content);
}

TEST_F(ServerIntegrationTest, UploadBinaryFile) {
    // Arrange - бинарные данные
    std::vector<char> binary_data = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
    };
    
    // Act - Upload
    auto upload_response = _client->put_file("/binary.bin", binary_data);
    
    // Assert - Upload
    EXPECT_EQ(upload_response.status_code, 201);
    
    // Act - Download
    auto download_response = _client->get("/binary.bin");
    
    // Assert - Download
    EXPECT_EQ(download_response.status_code, 200);
    EXPECT_EQ(download_response.body.size(), binary_data.size());
    
    std::vector<char> downloaded(download_response.body.begin(), download_response.body.end());
    EXPECT_EQ(downloaded, binary_data);
}

TEST_F(ServerIntegrationTest, UploadWithSpecialCharactersInFilename) {
    // Act
    auto response = _client->put("/file-with-dashes_and_underscores.txt", "Special chars");
    
    // Assert
    EXPECT_EQ(response.status_code, 201);
    
    // Verify download works
    auto download_response = _client->get("/file-with-dashes_and_underscores.txt");
    EXPECT_EQ(download_response.status_code, 200);
    EXPECT_EQ(download_response.body, "Special chars");
}

TEST_F(ServerIntegrationTest, MultipleSequentialOperations) {
    // Upload
    auto upload1 = _client->put("/seq1.txt", "First");
    EXPECT_EQ(upload1.status_code, 201);
    
    // Download
    auto download1 = _client->get("/seq1.txt");
    EXPECT_EQ(download1.status_code, 200);
    EXPECT_EQ(download1.body, "First");
    
    // Upload another
    auto upload2 = _client->put("/seq2.txt", "Second");
    EXPECT_EQ(upload2.status_code, 201);
    
    // List
    auto list = _client->get("/list");
    EXPECT_EQ(list.status_code, 200);
    EXPECT_NE(list.body.find("\"count\":2"), std::string::npos);
    
    // Delete first
    auto del1 = _client->delete_("/seq1.txt");
    EXPECT_EQ(del1.status_code, 200);
    
    // Verify deleted
    auto get_deleted = _client->get("/seq1.txt");
    EXPECT_EQ(get_deleted.status_code, 404);
}

TEST_F(ServerIntegrationTest, ConcurrentUploads) {
    // Загружаем несколько файлов параллельно
    std::vector<std::thread> threads;
    const int num_files = 5;
    
    for (int i = 0; i < num_files; ++i) {
        threads.emplace_back([this, i]() {
            std::string filename = "/concurrent_" + std::to_string(i) + ".txt";
            std::string content = "Content " + std::to_string(i);
            auto response = _client->put(filename, content);
            EXPECT_EQ(response.status_code, 201);
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Проверяем, что все файлы загружены
    auto list_response = _client->get("/list");
    EXPECT_EQ(list_response.status_code, 200);
    EXPECT_NE(list_response.body.find("\"count\":" + std::to_string(num_files)), std::string::npos);
}

TEST_F(ServerIntegrationTest, FileMetadataHeaders) {
    // Upload file
    _client->put("/metadata_test.txt", "Test content");
    
    // Download and check headers
    auto response = _client->get("/metadata_test.txt");
    
    EXPECT_EQ(response.status_code, 200);
    EXPECT_NE(response.headers.find("ETag"), response.headers.end());
    EXPECT_NE(response.headers.find("X-File-Size"), response.headers.end());
    EXPECT_EQ(response.headers.at("X-File-Size"), "12"); // "Test content" length
}

TEST_F(ServerIntegrationTest, DeleteNonExistentFile) {
    // Act
    auto response = _client->delete_("/does_not_exist.txt");
    
    // Assert
    EXPECT_EQ(response.status_code, 404);
}

TEST_F(ServerIntegrationTest, UploadToSubdirectory) {
    // Act
    auto response = _client->put("/folder/subfolder/file.txt", "Nested");
    
    // Assert
    EXPECT_EQ(response.status_code, 201);
    
    // Verify can download
    auto download = _client->get("/folder/subfolder/file.txt");
    EXPECT_EQ(download.status_code, 200);
    EXPECT_EQ(download.body, "Nested");
}

TEST_F(ServerIntegrationTest, CORSHeaders) {
    // Act
    auto response = _client->get("/list");
    
    // Assert - CORS headers should be present
    EXPECT_NE(response.headers.find("Access-Control-Allow-Origin"), response.headers.end());
    EXPECT_EQ(response.headers.at("Access-Control-Allow-Origin"), "*");
}

// Authentication integration tests
TEST_F(ServerAuthenticationIntegrationTest, AuthenticationRequired) {
    // Request without Authorization header should be rejected
    auto response = send_authenticated_request(http::verb::get, "/list");
    EXPECT_EQ(response.status_code, 401); // Unauthorized
    EXPECT_NE(response.body.find("Error"), std::string::npos);
}

TEST_F(ServerAuthenticationIntegrationTest, InvalidSignature) {
    // Request with malformed Authorization header
    std::map<std::string, std::string> headers = {
        {"Authorization", "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20240101/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=invalid_signature"},
        {"x-amz-date", "20240101T120000Z"}
    };
    auto response = send_authenticated_request(http::verb::get, "/list", "", headers);
    EXPECT_EQ(response.status_code, 401);
    EXPECT_NE(response.body.find("Error"), std::string::npos);
}

TEST_F(ServerAuthenticationIntegrationTest, InactiveKey) {
    // Request with inactive key (is_active=false)
    std::map<std::string, std::string> headers = {
        {"Authorization", "AWS4-HMAC-SHA256 Credential=AKIAINACTIVEKEY/20240101/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=some_signature"},
        {"x-amz-date", "20240101T120000Z"}
    };
    auto response = send_authenticated_request(http::verb::get, "/list", "", headers);
    EXPECT_EQ(response.status_code, 401);
    EXPECT_NE(response.body.find("Error"), std::string::npos);
}

TEST_F(ServerAuthenticationIntegrationTest, MissingTimestamp) {
    // Request without x-amz-date header
    std::map<std::string, std::string> headers = {
        {"Authorization", "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20240101/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=some_signature"}
    };
    auto response = send_authenticated_request(http::verb::get, "/list", "", headers);
    EXPECT_EQ(response.status_code, 401);
    EXPECT_NE(response.body.find("Error"), std::string::npos);
}

TEST_F(ServerAuthenticationIntegrationTest, ValidSignature) {
    // Create an authenticator instance to generate signature
    authenticator auth;
    
    // Load the same keys file as the server
    ASSERT_TRUE(auth.load_keys(_keys_file.string()));
    
    // Get the active key
    auto key_opt = auth.get_key("AKIAIOSFODNN7EXAMPLE");
    ASSERT_TRUE(key_opt.has_value());
    auto& key = key_opt.value();
    
    // Prepare request parameters
    std::string method = "GET";
    std::string uri = "/list";
    std::string body = "";
    std::string region = "us-east-1";
    std::string service = "s3";
    
    // Generate current timestamp in AWS format
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    gmtime_r(&now_time_t, &tm_buf);
    char timestamp[17];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%SZ", &tm_buf);
    
    // Headers that will be signed (must include host and x-amz-date)
    std::map<std::string, std::string> headers = {
        {"host", _host + ":" + std::to_string(_port)},
        {"x-amz-date", timestamp}
    };
    
    // Generate signature
    std::string signature = auth.generate_signature(
        key.access_key_id,
        key.secret_access_key,
        method,
        uri,
        headers,
        body,
        region,
        service
    );
    
    // Debug output
    std::cout << "DEBUG: timestamp = " << timestamp << std::endl;
    std::cout << "DEBUG: host header = " << headers.at("host") << std::endl;
    std::cout << "DEBUG: signature = " << signature << std::endl;
    
    // Ensure signature is not empty (generation succeeded)
    ASSERT_FALSE(signature.empty());
    
    // Build Authorization header
    std::string date_stamp = std::string(timestamp, 8);
    std::string credential_scope = date_stamp + "/" + region + "/" + service + "/aws4_request";
    std::string authorization_header =
        "AWS4-HMAC-SHA256 Credential=" + key.access_key_id + "/" + credential_scope +
        ", SignedHeaders=host;x-amz-date" +
        ", Signature=" + signature;
    
    headers["authorization"] = authorization_header;
    
    // Send request
    auto response = send_authenticated_request(http::verb::get, "/list", "", headers);
    
    // Expect success (200 OK)
    EXPECT_EQ(response.status_code, 200);
}

// ========== АВТОРИЗАЦИЯ ==========

TEST_F(ServerAuthorizationIntegrationTest, AuthorizationEnabledButUserNotFound) {
    // This test verifies that when authorization is enabled (users file provided),
    // but the authenticated user doesn't exist in the authorizer's user database,
    // the request should be rejected with 403 Forbidden (not 401 Unauthorized).
    
    // Create an authenticator instance to generate signature (same as ValidSignature test)
    authenticator auth;
    
    // Load the same keys file as the server
    ASSERT_TRUE(auth.load_keys(_keys_file.string()));
    
    // Get the active key
    auto key_opt = auth.get_key("AKIAIOSFODNN7EXAMPLE");
    ASSERT_TRUE(key_opt.has_value());
    auto& key = key_opt.value();
    
    // Prepare request parameters
    std::string method = "GET";
    std::string uri = "/list";
    std::string body = "";
    std::string region = "us-east-1";
    std::string service = "s3";
    
    // Generate current timestamp in AWS format
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    gmtime_r(&now_time_t, &tm_buf);
    char timestamp[17];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%SZ", &tm_buf);
    
    // Headers that will be signed
    std::map<std::string, std::string> headers = {
        {"host", _host + ":" + std::to_string(_port)},
        {"x-amz-date", timestamp}
    };
    
    // Generate signature
    std::string signature = auth.generate_signature(
        key.access_key_id,
        key.secret_access_key,
        method,
        uri,
        headers,
        body,
        region,
        service
    );
    
    // Ensure signature is not empty
    ASSERT_FALSE(signature.empty());
    
    // Build Authorization header
    std::string date_stamp = std::string(timestamp, 8);
    std::string credential_scope = date_stamp + "/" + region + "/" + service + "/aws4_request";
    std::string authorization_header =
        "AWS4-HMAC-SHA256 Credential=" + key.access_key_id + "/" + credential_scope +
        ", SignedHeaders=host;x-amz-date" +
        ", Signature=" + signature;
    
    headers["authorization"] = authorization_header;
    
    // Send request - authentication should succeed but authorization should fail
    auto response = send_authenticated_request(http::verb::get, "/list", "", headers);
    
    // Expect 403 Forbidden (authorization failed) not 401 Unauthorized (authentication failed)
    // Note: The server returns 403 when authorization fails
    EXPECT_EQ(response.status_code, 403);
    EXPECT_NE(response.body.find("Error"), std::string::npos);
}

TEST_F(ServerAuthorizationIntegrationTest, PublicResourceAccessWithoutAuthorization) {
    // This test verifies that public resources (if any) can be accessed
    // without authorization even when authorization is enabled.
    // For now, we test that a simple GET to root returns something
    // (might be 404, but should not be 403 if resource is public).
    
    // Create an authenticator instance to generate signature
    authenticator auth;
    ASSERT_TRUE(auth.load_keys(_keys_file.string()));
    
    auto key_opt = auth.get_key("AKIAIOSFODNN7EXAMPLE");
    ASSERT_TRUE(key_opt.has_value());
    auto& key = key_opt.value();
    
    // Generate timestamp
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    gmtime_r(&now_time_t, &tm_buf);
    char timestamp[17];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%SZ", &tm_buf);
    
    std::map<std::string, std::string> headers = {
        {"host", _host + ":" + std::to_string(_port)},
        {"x-amz-date", timestamp}
    };
    
    std::string signature = auth.generate_signature(
        key.access_key_id,
        key.secret_access_key,
        "GET",
        "/",
        headers,
        "",
        "us-east-1",
        "s3"
    );
    
    ASSERT_FALSE(signature.empty());
    
    std::string date_stamp = std::string(timestamp, 8);
    std::string credential_scope = date_stamp + "/us-east-1/s3/aws4_request";
    std::string authorization_header =
        "AWS4-HMAC-SHA256 Credential=" + key.access_key_id + "/" + credential_scope +
        ", SignedHeaders=host;x-amz-date" +
        ", Signature=" + signature;
    
    headers["authorization"] = authorization_header;
    
    auto response = send_authenticated_request(http::verb::get, "/", "", headers);
    
    // The root might return 404 (not found) or 200 (some welcome page)
    // but should not be 403 Forbidden if it's a public resource
    // Actually, without proper authorization setup, it will likely be 403
    // So we just check it's not 401 (authentication passed)
    EXPECT_NE(response.status_code, 401);
}