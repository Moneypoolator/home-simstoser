#include <gtest/gtest.h>
#include "test_http_client.hpp"
#include "server.hpp"
#include <thread>
#include <chrono>
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
    _client->put("/file2.txt", "Content 2");
    _client->put("/subdir/file3.txt", "Content 3");
    
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