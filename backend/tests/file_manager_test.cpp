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

// ========== СТРИМИНГОВАЯ ЗАГРУЗКА ==========

TEST_F(FileManagerTest, StreamUploadBasic) {
    // Arrange
    std::string filename = "stream_test.txt";
    
    // Act - Initiate stream upload
    auto stream_id_opt = _file_manager->initiate_stream_upload(filename);
    ASSERT_TRUE(stream_id_opt.has_value());
    std::string stream_id = stream_id_opt.value();
    
    // Write data in chunks
    std::vector<char> chunk1 = {'H', 'e', 'l', 'l', 'o', ' '};
    std::vector<char> chunk2 = {'W', 'o', 'r', 'l', 'd', '!'};
    
    bool write1 = _file_manager->write_to_stream(stream_id, chunk1);
    bool write2 = _file_manager->write_to_stream(stream_id, chunk2);
    
    // Check progress
    auto progress_opt = _file_manager->get_stream_upload_progress(stream_id);
    ASSERT_TRUE(progress_opt.has_value());
    EXPECT_EQ(progress_opt.value(), chunk1.size() + chunk2.size());
    
    // Complete upload
    bool completed = _file_manager->complete_stream_upload(stream_id);
    
    // Verify file was created
    auto downloaded = _file_manager->download_file(filename);
    
    // Assert
    EXPECT_TRUE(write1);
    EXPECT_TRUE(write2);
    EXPECT_TRUE(completed);
    ASSERT_TRUE(downloaded.has_value());
    
    std::vector<char> expected;
    expected.insert(expected.end(), chunk1.begin(), chunk1.end());
    expected.insert(expected.end(), chunk2.begin(), chunk2.end());
    
    EXPECT_EQ(downloaded.value(), expected);
}

TEST_F(FileManagerTest, StreamUploadEmptyFile) {
    // Arrange
    std::string filename = "empty_stream.txt";
    
    // Act
    auto stream_id_opt = _file_manager->initiate_stream_upload(filename);
    ASSERT_TRUE(stream_id_opt.has_value());
    std::string stream_id = stream_id_opt.value();
    
    // Write no data (empty file)
    bool completed = _file_manager->complete_stream_upload(stream_id);
    
    // Verify
    auto downloaded = _file_manager->download_file(filename);
    
    // Assert
    EXPECT_TRUE(completed);
    ASSERT_TRUE(downloaded.has_value());
    EXPECT_TRUE(downloaded.value().empty());
}

TEST_F(FileManagerTest, StreamUploadLargeChunks) {
    // Arrange
    std::string filename = "large_stream.bin";
    
    // Create large data (10KB)
    const size_t chunk_size = 10240;
    std::vector<char> large_data(chunk_size);
    for (size_t i = 0; i < chunk_size; ++i) {
        large_data[i] = static_cast<char>(i % 256);
    }
    
    // Act
    auto stream_id_opt = _file_manager->initiate_stream_upload(filename);
    ASSERT_TRUE(stream_id_opt.has_value());
    std::string stream_id = stream_id_opt.value();
    
    // Write in 5 smaller chunks
    const size_t num_chunks = 5;
    const size_t chunk_part_size = chunk_size / num_chunks;
    bool all_writes_success = true;
    
    for (size_t i = 0; i < num_chunks; ++i) {
        std::vector<char> chunk(
            large_data.begin() + i * chunk_part_size,
            large_data.begin() + (i + 1) * chunk_part_size
        );
        bool success = _file_manager->write_to_stream(stream_id, chunk);
        all_writes_success = all_writes_success && success;
    }
    
    bool completed = _file_manager->complete_stream_upload(stream_id);
    auto downloaded = _file_manager->download_file(filename);
    
    // Assert
    EXPECT_TRUE(all_writes_success);
    EXPECT_TRUE(completed);
    ASSERT_TRUE(downloaded.has_value());
    EXPECT_EQ(downloaded.value(), large_data);
}

TEST_F(FileManagerTest, StreamUploadAbort) {
    // Arrange
    std::string filename = "aborted_stream.txt";
    
    // Act
    auto stream_id_opt = _file_manager->initiate_stream_upload(filename);
    ASSERT_TRUE(stream_id_opt.has_value());
    std::string stream_id = stream_id_opt.value();
    
    // Write some data
    std::vector<char> data = {'t', 'e', 's', 't'};
    bool write_success = _file_manager->write_to_stream(stream_id, data);
    
    // Abort the upload
    bool abort_success = _file_manager->abort_stream_upload(stream_id);
    
    // Try to write after abort (should fail)
    bool write_after_abort = _file_manager->write_to_stream(stream_id, data);
    
    // Try to complete after abort (should fail)
    bool complete_after_abort = _file_manager->complete_stream_upload(stream_id);
    
    // Verify file doesn't exist
    bool file_exists = _file_manager->file_exists(filename);
    
    // Assert
    EXPECT_TRUE(write_success);
    EXPECT_TRUE(abort_success);
    EXPECT_FALSE(write_after_abort);  // Stream no longer exists
    EXPECT_FALSE(complete_after_abort);  // Stream no longer exists
    EXPECT_FALSE(file_exists);  // File should not be created
}

TEST_F(FileManagerTest, StreamUploadInvalidStreamId) {
    // Arrange
    std::string invalid_stream_id = "non_existent_stream_id";
    std::vector<char> data = {'t', 'e', 's', 't'};
    
    // Act & Assert
    // Write to non-existent stream
    bool write_success = _file_manager->write_to_stream(invalid_stream_id, data);
    EXPECT_FALSE(write_success);
    
    // Complete non-existent stream
    bool complete_success = _file_manager->complete_stream_upload(invalid_stream_id);
    EXPECT_FALSE(complete_success);
    
    // Abort non-existent stream
    bool abort_success = _file_manager->abort_stream_upload(invalid_stream_id);
    EXPECT_FALSE(abort_success);
    
    // Get progress of non-existent stream
    auto progress = _file_manager->get_stream_upload_progress(invalid_stream_id);
    EXPECT_FALSE(progress.has_value());
}

TEST_F(FileManagerTest, StreamUploadDoubleComplete) {
    // Arrange
    std::string filename = "double_complete.txt";
    
    // Act
    auto stream_id_opt = _file_manager->initiate_stream_upload(filename);
    ASSERT_TRUE(stream_id_opt.has_value());
    std::string stream_id = stream_id_opt.value();
    
    // Write data
    std::vector<char> data = {'d', 'a', 't', 'a'};
    bool write_success = _file_manager->write_to_stream(stream_id, data);
    
    // Complete first time
    bool complete1 = _file_manager->complete_stream_upload(stream_id);
    
    // Try to complete again (should fail)
    bool complete2 = _file_manager->complete_stream_upload(stream_id);
    
    // Try to write after completion (should fail)
    bool write_after_complete = _file_manager->write_to_stream(stream_id, data);
    
    // Assert
    EXPECT_TRUE(write_success);
    EXPECT_TRUE(complete1);
    EXPECT_FALSE(complete2);  // Already completed
    EXPECT_FALSE(write_after_complete);  // Already completed
}

TEST_F(FileManagerTest, StreamUploadUnsafePath) {
    // Arrange - try to upload with path traversal
    std::string unsafe_filename = "../etc/passwd";
    
    // Act
    auto stream_id_opt = _file_manager->initiate_stream_upload(unsafe_filename);
    
    // Assert
    EXPECT_FALSE(stream_id_opt.has_value());  // Should reject unsafe path
}

TEST_F(FileManagerTest, StreamUploadWithSubdirectory) {
    // Arrange
    std::string filename = "subdir/stream_file.txt";
    std::vector<char> data = {'s', 'u', 'b', 'd', 'i', 'r'};
    
    // Act
    auto stream_id_opt = _file_manager->initiate_stream_upload(filename);
    ASSERT_TRUE(stream_id_opt.has_value());
    std::string stream_id = stream_id_opt.value();
    
    bool write_success = _file_manager->write_to_stream(stream_id, data);
    bool complete_success = _file_manager->complete_stream_upload(stream_id);
    
    // Verify
    auto downloaded = _file_manager->download_file(filename);
    
    // Assert
    EXPECT_TRUE(write_success);
    EXPECT_TRUE(complete_success);
    ASSERT_TRUE(downloaded.has_value());
    EXPECT_EQ(downloaded.value(), data);
}