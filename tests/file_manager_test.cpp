#include <gtest/gtest.h>
#include "file_manager.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <functional>

#include <random>
#include <cstdint>
namespace fs = std::filesystem;

class FileManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Создаем временную директорию для тестов
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis;
        uint64_t random = dis(gen);
        _temp_dir = fs::temp_directory_path() / ("s3_test_" + std::to_string(random));
        fs::create_directories(_temp_dir);
        
        _file_manager = std::make_unique<file_manager>(_temp_dir.string());
    }
    
    void TearDown() override {
        // Удаляем временную директорию
        if (fs::exists(_temp_dir)) {
            fs::remove_all(_temp_dir);
        }
    }
    
    std::unique_ptr<file_manager> _file_manager;
    fs::path _temp_dir;
};

TEST_F(FileManagerTest, UploadAndDownloadFile) {
    // Arrange
    std::string filename = "test.txt";
    std::vector<char> data = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
    
    // Act
    bool uploaded = _file_manager->upload_file(filename, data);
    auto downloaded = _file_manager->download_file(filename);
    
    // Assert
    EXPECT_TRUE(uploaded);
    ASSERT_TRUE(downloaded.has_value());
    EXPECT_EQ(downloaded.value(), data);
}

TEST_F(FileManagerTest, FileExistsAfterUpload) {
    // Arrange
    std::string filename = "exists.txt";
    std::vector<char> data = {'T', 'e', 's', 't'};
    
    // Act
    _file_manager->upload_file(filename, data);
    bool exists = _file_manager->file_exists(filename);
    
    // Assert
    EXPECT_TRUE(exists);
}

TEST_F(FileManagerTest, FileDoesNotExist) {
    // Act
    bool exists = _file_manager->file_exists("nonexistent.txt");
    
    // Assert
    EXPECT_FALSE(exists);
}

TEST_F(FileManagerTest, DeleteFile) {
    // Arrange
    std::string filename = "todelete.txt";
    std::vector<char> data = {'D', 'e', 'l', 'e', 't', 'e'};
    _file_manager->upload_file(filename, data);
    
    // Act
    bool deleted = _file_manager->delete_file(filename);
    bool exists_after = _file_manager->file_exists(filename);
    
    // Assert
    EXPECT_TRUE(deleted);
    EXPECT_FALSE(exists_after);
}

TEST_F(FileManagerTest, GetMetadata) {
    // Arrange
    std::string filename = "metadata.txt";
    std::vector<char> data = {'M', 'e', 't', 'a'};
    _file_manager->upload_file(filename, data);
    
    // Act
    auto metadata = _file_manager->get_metadata(filename);
    
    // Assert
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->name, filename);
    EXPECT_EQ(metadata->size, data.size());
    EXPECT_FALSE(metadata->etag.empty());
}

TEST_F(FileManagerTest, ListFilesEmpty) {
    // Act
    auto files = _file_manager->list_files();
    
    // Assert
    EXPECT_TRUE(files.empty());
}

TEST_F(FileManagerTest, ListFilesWithContent) {
    // Arrange
    _file_manager->upload_file("file1.txt", std::vector<char>{'1'});
    _file_manager->upload_file("file2.txt", std::vector<char>{'2', '2'});
    _file_manager->upload_file("subdir/file3.txt", std::vector<char>{'3', '3', '3'});
    
    // Act
    auto files = _file_manager->list_files();
    
    // Assert
    EXPECT_EQ(files.size(), 3);
    
    // Проверяем, что все файлы присутствуют
    std::set<std::string> filenames;
    for (const auto& file : files) {
        filenames.insert(file.name);
    }
    
    EXPECT_TRUE(filenames.count("file1.txt") > 0);
    EXPECT_TRUE(filenames.count("file2.txt") > 0);
    EXPECT_TRUE(filenames.count("subdir/file3.txt") > 0);
}

TEST_F(FileManagerTest, PathTraversalProtection) {
    // Arrange
    std::vector<char> data = {'M', 'a', 'l', 'i', 'c', 'i', 'o', 'u', 's'};
    
    // Act & Assert - все эти попытки должны провалиться
    EXPECT_FALSE(_file_manager->upload_file("../outside.txt", data));
    EXPECT_FALSE(_file_manager->upload_file("/absolute/path.txt", data));
    EXPECT_FALSE(_file_manager->upload_file("dir/../../escape.txt", data));
    EXPECT_TRUE(_file_manager->upload_file("..\\windows_escape.txt", data));
}

TEST_F(FileManagerTest, ETagConsistency) {
    // Arrange
    std::string filename = "etag_test.txt";
    std::vector<char> data = {'S', 'a', 'm', 'e', ' ', 'c', 'o', 'n', 't', 'e', 'n', 't'};
    
    // Act
    _file_manager->upload_file(filename, data);
    auto metadata1 = _file_manager->get_metadata(filename);
    auto metadata2 = _file_manager->get_metadata(filename);
    
    // Assert
    ASSERT_TRUE(metadata1.has_value());
    ASSERT_TRUE(metadata2.has_value());
    EXPECT_EQ(metadata1->etag, metadata2->etag);
}

TEST_F(FileManagerTest, ETagDifferentContent) {
    // Arrange
    std::vector<char> data1 = {'C', 'o', 'n', 't', 'e', 'n', 't', '1'};
    std::vector<char> data2 = {'C', 'o', 'n', 't', 'e', 'n', 't', '2'};
    
    // Act
    _file_manager->upload_file("file1.txt", data1);
    _file_manager->upload_file("file2.txt", data2);
    
    auto metadata1 = _file_manager->get_metadata("file1.txt");
    auto metadata2 = _file_manager->get_metadata("file2.txt");
    
    // Assert
    ASSERT_TRUE(metadata1.has_value());
    ASSERT_TRUE(metadata2.has_value());
    EXPECT_NE(metadata1->etag, metadata2->etag);
}

TEST_F(FileManagerTest, LargeFileUpload) {
    // Arrange - создаем большой файл (1 МБ)
    std::string filename = "large.bin";
    std::vector<char> data(1024 * 1024); // 1 MB
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<char>(i % 256);
    }
    
    // Act
    bool uploaded = _file_manager->upload_file(filename, data);
    auto downloaded = _file_manager->download_file(filename);
    
    // Assert
    EXPECT_TRUE(uploaded);
    ASSERT_TRUE(downloaded.has_value());
    EXPECT_EQ(downloaded.value().size(), data.size());
    EXPECT_EQ(downloaded.value(), data);
}

TEST_F(FileManagerTest, UnicodeFilename) {
    // Arrange
    std::string filename = "тест_文件_テスト.txt";
    std::vector<char> data = {'U', 'n', 'i', 'c', 'o', 'd', 'e'};
    
    // Act
    bool uploaded = _file_manager->upload_file(filename, data);
    auto downloaded = _file_manager->download_file(filename);
    
    // Assert
    EXPECT_TRUE(uploaded);
    ASSERT_TRUE(downloaded.has_value());
    EXPECT_EQ(downloaded.value(), data);
}