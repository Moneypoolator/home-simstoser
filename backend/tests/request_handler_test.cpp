#include <gtest/gtest.h>
#include "request_handler.hpp"
#include "file_manager.hpp"
#include <filesystem>
#include <boost/beast/http.hpp>

namespace fs = std::filesystem;
namespace http = boost::beast::http;

class RequestHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        _temp_dir = fs::temp_directory_path() / "s3_test_handler";
        fs::create_directories(_temp_dir);
        
        _file_manager = std::make_unique<file_manager>(_temp_dir.string());
        _handler = std::make_unique<request_handler>(*_file_manager);
    }
    
    void TearDown() override {
        if (fs::exists(_temp_dir)) {
            fs::remove_all(_temp_dir);
        }
    }
    
    std::unique_ptr<file_manager> _file_manager;
    std::unique_ptr<request_handler> _handler;
    fs::path _temp_dir;
};

// Тест для метода создания ответа
TEST_F(RequestHandlerTest, CreateResponse) {
    http::response<http::string_body> response = _handler->create_response(
        http::status::ok,
        R"({"success": true})",
        "application/json"
    );
    
    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response.body(), R"({"success": true})");
    EXPECT_EQ(response[http::field::content_type], "application/json");
}

// Тест для извлечения имени файла из пути
TEST_F(RequestHandlerTest, GetFilenameFromPath) {
    EXPECT_EQ(_handler->get_filename_from_path("/test.txt"), "test.txt");
    EXPECT_EQ(_handler->get_filename_from_path("/folder/file.txt"), "folder/file.txt");
    EXPECT_EQ(_handler->get_filename_from_path("/sub/dir/file.txt"), "sub/dir/file.txt");
    EXPECT_EQ(_handler->get_filename_from_path("test.txt"), "test.txt");
    EXPECT_EQ(_handler->get_filename_from_path("/list"), "");
    EXPECT_EQ(_handler->get_filename_from_path("/list?param=value"), "");
}

TEST_F(RequestHandlerTest, GetFilenameWithQueryParams) {
    EXPECT_EQ(_handler->get_filename_from_path("/file.txt?version=1"), "file.txt");
    EXPECT_EQ(_handler->get_filename_from_path("/file.txt?download=true&size=100"), "file.txt");
}

TEST_F(RequestHandlerTest, GetFilenameSpecialChars) {
    EXPECT_EQ(_handler->get_filename_from_path("/file%20with%20spaces.txt"), "file with spaces.txt");
    EXPECT_EQ(_handler->get_filename_from_path("/file-with-dashes.txt"), "file-with-dashes.txt");
    EXPECT_EQ(_handler->get_filename_from_path("/file_with_underscores.txt"), "file_with_underscores.txt");
}

TEST_F(RequestHandlerTest, HandleListEmpty) {
    http::request<http::string_body> req{http::verb::get, "/list", 11};
    
    http::response<http::string_body> response = _handler->handle_list(req);
    
    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response[http::field::content_type], "application/json");
    
    // Парсим ответ
    auto body = response.body();
    EXPECT_NE(body.find("\"count\":0"), std::string::npos);
    EXPECT_NE(body.find("\"files\":[]"), std::string::npos);
}

TEST_F(RequestHandlerTest, HandleListWithFiles) {
    // Загружаем несколько файлов
    _file_manager->upload_file("file1.txt", std::vector<char>{'1'});
    _file_manager->upload_file("file2.txt", std::vector<char>{'2'});
    _file_manager->upload_file("subdir/file3.txt", std::vector<char>{'3'});
    
    http::request<http::string_body> req{http::verb::get, "/list", 11};
    
    http::response<http::string_body> response = _handler->handle_list(req);
    
    EXPECT_EQ(response.result(), http::status::ok);
    
    auto body = response.body();
    EXPECT_NE(body.find("\"count\":3"), std::string::npos);
    EXPECT_NE(body.find("file1.txt"), std::string::npos);
    EXPECT_NE(body.find("file2.txt"), std::string::npos);
    EXPECT_NE(body.find("subdir/file3.txt"), std::string::npos);
}

TEST_F(RequestHandlerTest, HandleGetFileNotFound) {
    http::request<http::string_body> req{http::verb::get, "/nonexistent.txt", 11};
    
    http::response<http::string_body> response = _handler->handle_get(req);
    
    EXPECT_EQ(response.result(), http::status::not_found);
    EXPECT_NE(response.body().find("File not found"), std::string::npos);
}

TEST_F(RequestHandlerTest, HandleGetFileExists) {
    // Загружаем файл
    std::vector<char> content = {'T', 'e', 's', 't', ' ', 'c', 'o', 'n', 't', 'e', 'n', 't'};
    _file_manager->upload_file("test.txt", content);
    
    http::request<http::string_body> req{http::verb::get, "/test.txt", 11};
    
    http::response<http::string_body> response = _handler->handle_get(req);
    
    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response.body(), std::string(content.begin(), content.end()));
    EXPECT_EQ(response[http::field::content_type], "application/octet-stream");
}

TEST_F(RequestHandlerTest, HandlePutFile) {
    http::request<http::string_body> req{http::verb::put, "/upload.txt", 11};
    req.body() = "Uploaded content";
    req.prepare_payload();
    
    http::response<http::string_body> response = _handler->handle_put(req);
    
    EXPECT_EQ(response.result(), http::status::created);
    EXPECT_TRUE(_file_manager->file_exists("upload.txt"));
    
    auto downloaded = _file_manager->download_file("upload.txt");
    ASSERT_TRUE(downloaded.has_value());
    EXPECT_EQ(downloaded.value(), std::vector<char>({'U', 'p', 'l', 'o', 'a', 'd', 'e', 'd', ' ', 'c', 'o', 'n', 't', 'e', 'n', 't'}));
}

TEST_F(RequestHandlerTest, HandlePutEmptyBody) {
    http::request<http::string_body> req{http::verb::put, "/empty.txt", 11};
    // Пустое тело
    
    http::response<http::string_body> response = _handler->handle_put(req);
    
    EXPECT_EQ(response.result(), http::status::bad_request);
    EXPECT_FALSE(_file_manager->file_exists("empty.txt"));
}

TEST_F(RequestHandlerTest, HandlePutWithSubdirectory) {
    http::request<http::string_body> req{http::verb::put, "/folder/subfolder/file.txt", 11};
    req.body() = "Nested content";
    req.prepare_payload();
    
    http::response<http::string_body> response = _handler->handle_put(req);
    
    EXPECT_EQ(response.result(), http::status::created);
    EXPECT_TRUE(_file_manager->file_exists("folder/subfolder/file.txt"));
}

TEST_F(RequestHandlerTest, HandleDeleteFileNotFound) {
    http::request<http::string_body> req{http::verb::delete_, "/nonexistent.txt", 11};
    
    http::response<http::string_body> response = _handler->handle_delete(req);
    
    EXPECT_EQ(response.result(), http::status::not_found);
}

TEST_F(RequestHandlerTest, HandleDeleteFileExists) {
    // Загружаем файл
    _file_manager->upload_file("todelete.txt", std::vector<char>{'D'});
    
    http::request<http::string_body> req{http::verb::delete_, "/todelete.txt", 11};
    
    http::response<http::string_body> response = _handler->handle_delete(req);
    
    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_FALSE(_file_manager->file_exists("todelete.txt"));
}

TEST_F(RequestHandlerTest, HandlePutAndGetRoundtrip) {
    // PUT запрос
    http::request<http::string_body> put_req{http::verb::put, "/roundtrip.txt", 11};
    put_req.body() = "Roundtrip test data";
    put_req.prepare_payload();
    
    http::response<http::string_body> put_response = _handler->handle_put(put_req);
    EXPECT_EQ(put_response.result(), http::status::created);
    
    // GET запрос того же файла
    http::request<http::string_body> get_req{http::verb::get, "/roundtrip.txt", 11};
    http::response<http::string_body> get_response = _handler->handle_get(get_req);
    
    EXPECT_EQ(get_response.result(), http::status::ok);
    EXPECT_EQ(get_response.body(), "Roundtrip test data");
}

TEST_F(RequestHandlerTest, HandlePutDeleteGetSequence) {
    // PUT
    http::request<http::string_body> put_req{http::verb::put, "/sequence.txt", 11};
    put_req.body() = "Sequence test";
    put_req.prepare_payload();
    _handler->handle_put(put_req);
    
    // DELETE
    http::request<http::string_body> del_req{http::verb::delete_, "/sequence.txt", 11};
    http::response<http::string_body> del_response = _handler->handle_delete(del_req);
    EXPECT_EQ(del_response.result(), http::status::ok);
    
    // GET (должен вернуть 404)
    http::request<http::string_body> get_req{http::verb::get, "/sequence.txt", 11};
    http::response<http::string_body> get_response = _handler->handle_get(get_req);
    EXPECT_EQ(get_response.result(), http::status::not_found);
}

TEST_F(RequestHandlerTest, HandlePutFileTooLarge) {
    // Создаём file_manager с маленьким лимитом
    upload_limits small_limits;
    small_limits.max_file_size = 10; // 10 байт
    file_manager fm_small(_temp_dir.string(), small_limits);
    request_handler handler_small(fm_small);
    
    http::request<http::string_body> req{http::verb::put, "/large.txt", 11};
    req.body() = "This is more than 10 bytes";
    req.prepare_payload();
    
    http::response<http::string_body> response = handler_small.handle_put(req);
    // Ожидаем, что file_manager вернёт false, и handler вернёт 500 (Internal Server Error)
    // Или можно ожидать 413, если мы доработаем обработку ошибок.
    // Пока в коде handle_put при ошибке upload_file возвращает 500.
    EXPECT_EQ(response.result(), http::status::internal_server_error);
}

// Аналогично для multipart upload
TEST_F(RequestHandlerTest, HandleUploadPartTooLarge) {
    upload_limits small_limits;
    small_limits.max_part_size = 10;
    file_manager fm_small(_temp_dir.string(), small_limits);
    request_handler handler_small(fm_small);
    
    // Инициируем загрузку
    auto upload_id = fm_small.initiate_multipart_upload("multi.txt");
    ASSERT_TRUE(upload_id.has_value());
    
    // Формируем запрос на загрузку части с большими данными
    std::string target = "/upload/part?upload_id=" + *upload_id + "&part_number=1";
    http::request<http::string_body> req{http::verb::put, target, 11};
    req.body() = "This part is too long";
    req.prepare_payload();
    
    http::response<http::string_body> response = handler_small.handle_upload_part(req);
    EXPECT_EQ(response.result(), http::status::internal_server_error);
    
    fm_small.abort_multipart_upload(*upload_id);
}

TEST_F(RequestHandlerTest, HandleGetWithRangeHeader) {
    // Upload a test file
    std::vector<char> data(2048);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<char>(i % 256);
    }
    _file_manager->upload_file("range_test.bin", data);
    
    // Request with Range: bytes=0-1023
    http::request<http::string_body> req{http::verb::get, "/range_test.bin", 11};
    req.set(http::field::range, "bytes=0-1023");
    
    http::response<http::string_body> response = _handler->handle_get(req);
    
    EXPECT_EQ(response.result(), http::status::partial_content);
    EXPECT_EQ(response[http::field::content_type], "application/octet-stream");
    EXPECT_EQ(response[http::field::accept_ranges], "bytes");
    EXPECT_EQ(response[http::field::content_range], "bytes 0-1023/2048");
    EXPECT_EQ(response.body().size(), 1024);
    
    // Verify content matches original data
    for (size_t i = 0; i < 1024; ++i) {
        EXPECT_EQ(static_cast<unsigned char>(response.body()[i]), static_cast<unsigned char>(data[i]));
    }
}

TEST_F(RequestHandlerTest, HandleGetWithRangeHeaderOutOfBounds) {
    // Upload a test file
    std::vector<char> data(100);
    _file_manager->upload_file("small.bin", data);
    
    // Range beyond file size
    http::request<http::string_body> req{http::verb::get, "/small.bin", 11};
    req.set(http::field::range, "bytes=200-300");
    
    http::response<http::string_body> response = _handler->handle_get(req);
    
    // Should return 416 Range Not Satisfiable
    EXPECT_EQ(response.result(), http::status::range_not_satisfiable);
    EXPECT_EQ(response[http::field::content_range], "bytes */100");
}

TEST_F(RequestHandlerTest, HandleGetWithInvalidRangeHeader) {
    std::vector<char> data(100);
    _file_manager->upload_file("test.bin", data);
    
    // Invalid range format
    http::request<http::string_body> req{http::verb::get, "/test.bin", 11};
    req.set(http::field::range, "bytes=invalid");
    
    http::response<http::string_body> response = _handler->handle_get(req);
    
    // Should ignore range and return full file (status ok)
    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response.body().size(), 100);
}

TEST_F(RequestHandlerTest, HandleGetWithSuffixRangeNotSupported) {
    std::vector<char> data(100);
    _file_manager->upload_file("test.bin", data);
    
    // Suffix range (last N bytes) not implemented
    http::request<http::string_body> req{http::verb::get, "/test.bin", 11};
    req.set(http::field::range, "bytes=-50");
    
    http::response<http::string_body> response = _handler->handle_get(req);
    
    // Should ignore range and return full file
    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response.body().size(), 100);
}

TEST_F(RequestHandlerTest, HandleGetWithoutRangeHeader) {
    std::vector<char> data(500);
    _file_manager->upload_file("full.bin", data);
    
    http::request<http::string_body> req{http::verb::get, "/full.bin", 11};
    // No range header
    
    http::response<http::string_body> response = _handler->handle_get(req);
    
    EXPECT_EQ(response.result(), http::status::ok);
    EXPECT_EQ(response[http::field::accept_ranges], "bytes");
    EXPECT_EQ(response.body().size(), 500);
}