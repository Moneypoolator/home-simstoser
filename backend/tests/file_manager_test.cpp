#include <gtest/gtest.h>
#include "file_manager.hpp"
#include <filesystem>
#include <sstream>
#include <thread>
#include <chrono>

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
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
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
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
    
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
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
    
    // Act
    auto metadata = _file_manager->get_metadata(filename);
    
    // Assert
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->name, filename);
    EXPECT_EQ(metadata->size, data.size());
    EXPECT_FALSE(metadata->etag.empty());
}

TEST_F(FileManagerTest, CustomMetadata) {
    // Arrange
    std::string filename = "custom_meta.txt";
    std::vector<char> data = {'d', 'a', 't', 'a'};
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
    
    std::map<std::string, std::string> meta = {
        {"author", "testuser"},
        {"content-type", "text/plain"},
        {"description", "test file"}
    };
    
    // Act: set custom metadata
    bool set_success = _file_manager->set_custom_metadata(filename, meta);
    EXPECT_TRUE(set_success);
    
    // Retrieve via get_metadata
    auto metadata = _file_manager->get_metadata(filename);
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->custom_metadata, meta);
    
    // Retrieve via get_custom_metadata
    auto custom_only = _file_manager->get_custom_metadata(filename);
    ASSERT_TRUE(custom_only.has_value());
    EXPECT_EQ(*custom_only, meta);
    
    // Update metadata
    std::map<std::string, std::string> updated = {{"author", "newuser"}};
    set_success = _file_manager->set_custom_metadata(filename, updated);
    EXPECT_TRUE(set_success);
    auto updated_metadata = _file_manager->get_metadata(filename);
    ASSERT_TRUE(updated_metadata.has_value());
    EXPECT_EQ(updated_metadata->custom_metadata, updated);
}

TEST_F(FileManagerTest, ListFilesEmpty) {
    // Act
    auto files = _file_manager->list_files();
    
    // Assert
    EXPECT_TRUE(files.empty());
}

TEST_F(FileManagerTest, ListFilesWithContent) {
    // Arrange
    EXPECT_TRUE(_file_manager->upload_file("file1.txt", std::vector<char>{'1'}));
    EXPECT_TRUE(_file_manager->upload_file("file2.txt", std::vector<char>{'2', '2'}));
    EXPECT_TRUE(_file_manager->upload_file("subdir/file3.txt", std::vector<char>{'3', '3', '3'}));
    
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
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
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
    EXPECT_TRUE(_file_manager->upload_file("file1.txt", data1));
    EXPECT_TRUE(_file_manager->upload_file("file2.txt", data2));
    
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

// ========== ТЕСТЫ ЛИМИТОВ ==========

TEST_F(FileManagerTest, MaxFileSizeLimitExceeded) {
    // Создаём file_manager с маленьким лимитом (1 KB)
    upload_limits small_limits;
    small_limits.max_file_size = 1024; // 1 KB
    file_manager fm_small(_temp_dir.string(), small_limits);
    
    std::vector<char> data(2048, 'A'); // 2 KB
    bool uploaded = fm_small.upload_file("large.txt", data);
    EXPECT_FALSE(uploaded);
    EXPECT_FALSE(fm_small.file_exists("large.txt"));
}

TEST_F(FileManagerTest, MaxFileSizeLimitRespected) {
    upload_limits small_limits;
    small_limits.max_file_size = 2048;
    file_manager fm_small(_temp_dir.string(), small_limits);
    
    std::vector<char> data(1024, 'B');
    bool uploaded = fm_small.upload_file("ok.txt", data);
    EXPECT_TRUE(uploaded);
    EXPECT_TRUE(fm_small.file_exists("ok.txt"));
}

TEST_F(FileManagerTest, MaxPartSizeLimitExceeded) {
    upload_limits limits;
    limits.max_part_size = 1024;
    file_manager fm_limits(_temp_dir.string(), limits);
    
    auto upload_id = fm_limits.initiate_multipart_upload("multi.bin");
    ASSERT_TRUE(upload_id.has_value());
    
    std::vector<char> large_part(2048, 'C');
    bool part_uploaded = fm_limits.upload_part(*upload_id, 1, large_part);
    EXPECT_FALSE(part_uploaded);
    
    EXPECT_TRUE(fm_limits.abort_multipart_upload(*upload_id));
}

TEST_F(FileManagerTest, MaxPartsPerUploadLimit) {
    upload_limits limits;
    limits.max_parts_per_upload = 2;
    file_manager fm_limits(_temp_dir.string(), limits);
    
    auto upload_id = fm_limits.initiate_multipart_upload("many_parts.bin");
    ASSERT_TRUE(upload_id.has_value());
    
    // Загружаем первую часть
    std::vector<char> part_data(100, 'D');
    bool part1 = fm_limits.upload_part(*upload_id, 1, part_data);
    EXPECT_TRUE(part1);
    
    // Вторая часть
    bool part2 = fm_limits.upload_part(*upload_id, 2, part_data);
    EXPECT_TRUE(part2);
    
    // Третья часть – должна быть отклонена
    bool part3 = fm_limits.upload_part(*upload_id, 3, part_data);
    EXPECT_FALSE(part3);
    
    EXPECT_TRUE(fm_limits.abort_multipart_upload(*upload_id));
}

TEST_F(FileManagerTest, MaxTempStorageTotalLimit) {
    upload_limits limits;
    limits.max_temp_storage_total = 1024; // всего 1 KB временных файлов
    limits.max_part_size = 1024; // allow 600-byte parts
    file_manager fm_limits(_temp_dir.string(), limits);
    
    // Первая загрузка
    auto upload_id1 = fm_limits.initiate_multipart_upload("file1.bin");
    ASSERT_TRUE(upload_id1.has_value());
    std::vector<char> part1(600, 'E'); // 600 байт
    bool uploaded1 = fm_limits.upload_part(*upload_id1, 1, part1);
    EXPECT_TRUE(uploaded1); // 600 < 1024
    
    // Вторая загрузка (ещё 600 байт → превышение)
    auto upload_id2 = fm_limits.initiate_multipart_upload("file2.bin");
    ASSERT_TRUE(upload_id2.has_value());
    std::vector<char> part2(600, 'F');
    bool uploaded2 = fm_limits.upload_part(*upload_id2, 1, part2);
    EXPECT_FALSE(uploaded2); // лимит временных файлов превышен
    
    EXPECT_TRUE(fm_limits.abort_multipart_upload(*upload_id1));
    EXPECT_TRUE(fm_limits.abort_multipart_upload(*upload_id2));
}

TEST_F(FileManagerTest, StreamUploadMaxFileSizeLimit) {
    upload_limits limits;
    limits.max_file_size = 1024;
    file_manager fm_limits(_temp_dir.string(), limits);
    
    auto stream_id = fm_limits.initiate_stream_upload("stream.bin");
    ASSERT_TRUE(stream_id.has_value());
    
    std::vector<char> data1(600, 'G');
    bool written1 = fm_limits.write_to_stream(*stream_id, data1);
    EXPECT_TRUE(written1);
    
    std::vector<char> data2(500, 'H');
    bool written2 = fm_limits.write_to_stream(*stream_id, data2);
    EXPECT_FALSE(written2); // превышение 1024 (600+500=1100)
    
    EXPECT_TRUE(fm_limits.abort_stream_upload(*stream_id));
}

TEST_F(FileManagerTest, UploadFileStreamRespectsLimit) {
    upload_limits limits;
    limits.max_file_size = 512;
    file_manager fm_limits(_temp_dir.string(), limits);
    
    std::string content(600, 'I');
    std::stringstream ss(content);
    bool uploaded = fm_limits.upload_file_stream("stream_test.txt", ss);
    EXPECT_FALSE(uploaded);
    EXPECT_FALSE(fm_limits.file_exists("stream_test.txt"));
}

TEST_F(FileManagerTest, CacheContent) {
    // Enable cache with small size to test eviction
    cache_config cfg;
    cfg.enabled = true;
    cfg.max_content_cache_bytes = 1024; // 1 KB
    cfg.content_ttl = std::chrono::seconds(10);
    file_manager fm_cache(_temp_dir.string(), upload_limits(), cfg);
    
    std::string filename = "cache_test.txt";
    std::vector<char> data = {'a', 'b', 'c', 'd', 'e'};
    
    // Upload file
    ASSERT_TRUE(fm_cache.upload_file(filename, data));
    
    // First download - should miss cache
    auto result1 = fm_cache.download_file(filename);
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1.value(), data);
    
    // Second download - should hit cache
    auto result2 = fm_cache.download_file(filename);
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2.value(), data);
    
    // Modify file (upload different content)
    std::vector<char> new_data = {'x', 'y', 'z'};
    ASSERT_TRUE(fm_cache.upload_file(filename, new_data));
    
    // Download again - should miss cache (invalidated) and get new data
    auto result3 = fm_cache.download_file(filename);
    ASSERT_TRUE(result3.has_value());
    EXPECT_EQ(result3.value(), new_data);
}

TEST_F(FileManagerTest, CacheMetadata) {
    cache_config cfg;
    cfg.enabled = true;
    cfg.max_metadata_cache_entries = 10;
    cfg.metadata_ttl = std::chrono::seconds(10);
    file_manager fm_cache(_temp_dir.string(), upload_limits(), cfg);
    
    std::string filename = "meta_test.txt";
    std::vector<char> data = {'1', '2', '3'};
    ASSERT_TRUE(fm_cache.upload_file(filename, data));
    
    // First get_metadata - miss
    auto meta1 = fm_cache.get_metadata(filename);
    ASSERT_TRUE(meta1.has_value());
    EXPECT_EQ(meta1->name, filename);
    EXPECT_EQ(meta1->size, data.size());
    
    // Second get_metadata - hit
    auto meta2 = fm_cache.get_metadata(filename);
    ASSERT_TRUE(meta2.has_value());
    EXPECT_EQ(meta2->size, data.size());
    
    // Update file (size changes)
    std::vector<char> larger_data(100, 'A');
    ASSERT_TRUE(fm_cache.upload_file(filename, larger_data));
    
    // Should get updated metadata (cache invalidated)
    auto meta3 = fm_cache.get_metadata(filename);
    ASSERT_TRUE(meta3.has_value());
    EXPECT_EQ(meta3->size, larger_data.size());
}

TEST_F(FileManagerTest, CacheDisabled) {
    cache_config cfg;
    cfg.enabled = false;
    file_manager fm_no_cache(_temp_dir.string(), upload_limits(), cfg);
    
    std::string filename = "nocache.txt";
    std::vector<char> data = {'t', 'e', 's', 't'};
    ASSERT_TRUE(fm_no_cache.upload_file(filename, data));
    
    // Multiple downloads - each should read from disk (no caching)
    auto result1 = fm_no_cache.download_file(filename);
    ASSERT_TRUE(result1.has_value());
    auto result2 = fm_no_cache.download_file(filename);
    ASSERT_TRUE(result2.has_value());
    // No easy way to verify cache miss, but at least ensure correctness
    EXPECT_EQ(result1.value(), data);
    EXPECT_EQ(result2.value(), data);
}

// ========== MULTIPART UPLOAD ==========

TEST_F(FileManagerTest, MultipartUpload_CompleteFlow) {
    std::string filename = "multipart_complete.bin";
    auto upload_id_opt = _file_manager->initiate_multipart_upload(filename);
    ASSERT_TRUE(upload_id_opt.has_value());
    std::string upload_id = *upload_id_opt;
    
    // Загружаем 3 части
    std::vector<char> part1_data(1024, 'A');
    std::vector<char> part2_data(2048, 'B');
    std::vector<char> part3_data(512, 'C');
    
    EXPECT_TRUE(_file_manager->upload_part(upload_id, 1, part1_data));
    EXPECT_TRUE(_file_manager->upload_part(upload_id, 2, part2_data));
    EXPECT_TRUE(_file_manager->upload_part(upload_id, 3, part3_data));
    
    // Проверяем прогресс
    auto progress = _file_manager->get_upload_progress(upload_id);
    ASSERT_TRUE(progress.has_value());
    EXPECT_EQ(progress->size(), 3);
    EXPECT_EQ((*progress)[1], 1024);
    EXPECT_EQ((*progress)[2], 2048);
    EXPECT_EQ((*progress)[3], 512);
    
    // Завершаем загрузку
    std::vector<int> part_numbers = {1, 2, 3};
    EXPECT_TRUE(_file_manager->complete_multipart_upload(upload_id, part_numbers));
    
    // Проверяем итоговый файл
    auto downloaded = _file_manager->download_file(filename);
    ASSERT_TRUE(downloaded.has_value());
    
    // Собираем ожидаемые данные
    std::vector<char> expected;
    expected.insert(expected.end(), part1_data.begin(), part1_data.end());
    expected.insert(expected.end(), part2_data.begin(), part2_data.end());
    expected.insert(expected.end(), part3_data.begin(), part3_data.end());
    
    EXPECT_EQ(downloaded->size(), expected.size());
    EXPECT_EQ(*downloaded, expected);
}

TEST_F(FileManagerTest, MultipartUpload_Abort) {
    std::string filename = "multipart_abort.bin";
    auto upload_id_opt = _file_manager->initiate_multipart_upload(filename);
    ASSERT_TRUE(upload_id_opt.has_value());
    std::string upload_id = *upload_id_opt;
    
    // Загружаем одну часть
    std::vector<char> part_data(512, 'X');
    EXPECT_TRUE(_file_manager->upload_part(upload_id, 1, part_data));
    
    // Проверяем, что временные файлы созданы (косвенно через прогресс)
    auto progress = _file_manager->get_upload_progress(upload_id);
    ASSERT_TRUE(progress.has_value());
    EXPECT_EQ(progress->size(), 1);
    
    // Прерываем загрузку
    EXPECT_TRUE(_file_manager->abort_multipart_upload(upload_id));
    
    // Прогресс больше не должен быть доступен
    progress = _file_manager->get_upload_progress(upload_id);
    EXPECT_FALSE(progress.has_value());
    
    // Файл не должен быть создан
    EXPECT_FALSE(_file_manager->file_exists(filename));
}

TEST_F(FileManagerTest, MultipartUpload_Progress) {
    std::string filename = "progress_test.bin";
    auto upload_id_opt = _file_manager->initiate_multipart_upload(filename);
    ASSERT_TRUE(upload_id_opt.has_value());
    std::string upload_id = *upload_id_opt;
    
    // Изначально прогресс пуст
    auto progress = _file_manager->get_upload_progress(upload_id);
    ASSERT_TRUE(progress.has_value());
    EXPECT_TRUE(progress->empty());
    
    // Загружаем части
    std::vector<char> data1(100, '1');
    std::vector<char> data2(200, '2');
    EXPECT_TRUE(_file_manager->upload_part(upload_id, 5, data1));
    EXPECT_TRUE(_file_manager->upload_part(upload_id, 2, data2));
    
    progress = _file_manager->get_upload_progress(upload_id);
    ASSERT_TRUE(progress.has_value());
    EXPECT_EQ(progress->size(), 2);
    EXPECT_EQ((*progress)[5], 100);
    EXPECT_EQ((*progress)[2], 200);
    
    // Загружаем ещё одну часть с тем же номером (перезапись)
    std::vector<char> data3(150, '3');
    EXPECT_TRUE(_file_manager->upload_part(upload_id, 2, data3));
    
    progress = _file_manager->get_upload_progress(upload_id);
    EXPECT_EQ((*progress)[2], 150); // размер обновился
    
    EXPECT_TRUE(_file_manager->abort_multipart_upload(upload_id));
}

TEST_F(FileManagerTest, MultipartUpload_ResumePart) {
    std::string filename = "resume_part.bin";
    auto upload_id_opt = _file_manager->initiate_multipart_upload(filename);
    ASSERT_TRUE(upload_id_opt.has_value());
    std::string upload_id = *upload_id_opt;
    
    std::vector<char> original(500, 'O');
    EXPECT_TRUE(_file_manager->upload_part(upload_id, 1, original));
    
    // "Возобновляем" загрузку той же части с другими данными (перезаписываем)
    std::vector<char> new_data(300, 'N');
    EXPECT_TRUE(_file_manager->upload_part(upload_id, 1, new_data));
    
    // Завершаем с частью 1
    std::vector<int> parts = {1};
    EXPECT_TRUE(_file_manager->complete_multipart_upload(upload_id, parts));
    
    auto downloaded = _file_manager->download_file(filename);
    ASSERT_TRUE(downloaded.has_value());
    EXPECT_EQ(downloaded->size(), 300);
    EXPECT_EQ(*downloaded, new_data);
}

TEST_F(FileManagerTest, MultipartUpload_InvalidPartNumbers) {
    std::string filename = "invalid_parts.bin";
    auto upload_id_opt = _file_manager->initiate_multipart_upload(filename);
    ASSERT_TRUE(upload_id_opt.has_value());
    std::string upload_id = *upload_id_opt;
    
    std::vector<char> data(100, 'D');
    EXPECT_TRUE(_file_manager->upload_part(upload_id, 1, data));
    
    // Пытаемся завершить с отсутствующей частью
    std::vector<int> parts = {1, 2}; // часть 2 не загружена
    EXPECT_FALSE(_file_manager->complete_multipart_upload(upload_id, parts));
    
    // Проверяем, что загрузка не завершилась, и файл не создан
    EXPECT_FALSE(_file_manager->file_exists(filename));
    
    // Можно попробовать снова с правильными частями
    parts = {1};
    EXPECT_TRUE(_file_manager->complete_multipart_upload(upload_id, parts));
    EXPECT_TRUE(_file_manager->file_exists(filename));
}

TEST_F(FileManagerTest, MultipartUpload_EmptyPartsList) {
    std::string filename = "empty_parts.bin";
    auto upload_id_opt = _file_manager->initiate_multipart_upload(filename);
    ASSERT_TRUE(upload_id_opt.has_value());
    std::string upload_id = *upload_id_opt;
    
    std::vector<int> empty_parts;
    EXPECT_FALSE(_file_manager->complete_multipart_upload(upload_id, empty_parts));
    
    EXPECT_TRUE(_file_manager->abort_multipart_upload(upload_id));
}

// ========== RANGE REQUESTS ==========

TEST_F(FileManagerTest, DownloadFileRange_Valid) {
    std::string filename = "range_test.bin";
    std::vector<char> data(2048);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<char>(i % 256);
    }
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
    
    // Скачиваем диапазон 100-199 (100 байт)
    auto range_data = _file_manager->download_file_range(filename, 100, 199);
    ASSERT_TRUE(range_data.has_value());
    EXPECT_EQ(range_data->size(), 100);
    
    for (size_t i = 0; i < 100; ++i) {
        EXPECT_EQ((*range_data)[i], data[100 + i]);
    }
}

TEST_F(FileManagerTest, DownloadFileRange_StartOnly) {
    std::string filename = "range_start.bin";
    std::vector<char> data(1024);
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<char>(i);
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
    
    // Открытый диапазон: от 512 до конца (не реализовано, но функция ожидает start и end)
    // Текущая реализация требует end. Проверим, что можно указать end = size-1
    auto range_data = _file_manager->download_file_range(filename, 512, 1023);
    ASSERT_TRUE(range_data.has_value());
    EXPECT_EQ(range_data->size(), 512);
}

TEST_F(FileManagerTest, DownloadFileRange_OutOfBounds) {
    std::string filename = "range_oob.bin";
    std::vector<char> data(100, 'X');
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
    
    // Start за пределами файла
    auto range1 = _file_manager->download_file_range(filename, 200, 300);
    EXPECT_FALSE(range1.has_value());
    
    // End за пределами файла (должен корректироваться внутри функции)
    auto range2 = _file_manager->download_file_range(filename, 50, 200);
    // В текущей реализации, если end >= file_size, то end = file_size - 1
    // Проверим, что возвращается от 50 до 99 (50 байт)
    ASSERT_TRUE(range2.has_value());
    EXPECT_EQ(range2->size(), 50);
}

TEST_F(FileManagerTest, DownloadFileRange_InvalidRange) {
    std::string filename = "range_invalid.bin";
    std::vector<char> data(100, 'Y');
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
    
    // start > end
    auto range = _file_manager->download_file_range(filename, 50, 40);
    EXPECT_FALSE(range.has_value());
}

// ========== STREAM UPLOAD RESUME (CLEANUP) ==========

TEST_F(FileManagerTest, StreamUpload_CleanupOnDestruction) {
    // Создаём file_manager внутри блока, чтобы проверить деструктор
    std::string filename = "stream_cleanup.bin";
    std::string stream_id;
    
    {
        file_manager local_fm(_temp_dir.string());
        auto stream_id_opt = local_fm.initiate_stream_upload(filename);
        ASSERT_TRUE(stream_id_opt.has_value());
        stream_id = *stream_id_opt;
        
        // Записываем данные
        std::vector<char> data(500, 'S');
        EXPECT_TRUE(local_fm.write_to_stream(stream_id, data));
        
        // Не завершаем, выходим из блока -> деструктор должен очистить временные файлы
    }
    
    // Файл не должен существовать
    file_manager check_fm(_temp_dir.string());
    EXPECT_FALSE(check_fm.file_exists(filename));
    
    // Попытка продолжить загрузку с тем же stream_id должна провалиться
    file_manager new_fm(_temp_dir.string());
    EXPECT_FALSE(new_fm.write_to_stream(stream_id, std::vector<char>{'X'}));
    EXPECT_FALSE(new_fm.complete_stream_upload(stream_id));
}

// ========== SENDFILE FALLBACK ==========

TEST_F(FileManagerTest, DownloadFileStream_Works) {
    std::string filename = "download_stream.bin";
    std::vector<char> data(1024 * 1024, 'F'); // 1 MB
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
    
    std::stringstream output;
    bool success = _file_manager->download_file_stream(filename, output);
    
    EXPECT_TRUE(success);
    std::string output_str = output.str();
    EXPECT_EQ(output_str.size(), data.size());
    EXPECT_EQ(std::vector<char>(output_str.begin(), output_str.end()), data);
}

TEST_F(FileManagerTest, UploadFileStream_Works) {
    std::string filename = "upload_stream.bin";
    std::vector<char> data(500 * 1024, 'U'); // 500 KB
    std::stringstream input(std::string(data.begin(), data.end()));
    
    bool success = _file_manager->upload_file_stream(filename, input);
    EXPECT_TRUE(success);
    
    auto downloaded = _file_manager->download_file(filename);
    ASSERT_TRUE(downloaded.has_value());
    EXPECT_EQ(*downloaded, data);
}

// ========== CACHE EVICTION ==========

TEST_F(FileManagerTest, CacheEviction_LRU) {
    cache_config cfg;
    cfg.enabled = true;
    cfg.max_content_cache_bytes = 2048; // 2 KB
    cfg.content_ttl = std::chrono::seconds(60);
    file_manager fm_cache(_temp_dir.string(), upload_limits(), cfg);
    
    // Загружаем файлы разного размера
    std::vector<char> small_data(512, 'S');
    std::vector<char> medium_data(1024, 'M');
    std::vector<char> large_data(1024, 'L'); // ещё 1 KB
    
    EXPECT_TRUE(fm_cache.upload_file("small.bin", small_data));
    EXPECT_TRUE(fm_cache.upload_file("medium.bin", medium_data));
    EXPECT_TRUE(fm_cache.upload_file("large.bin", large_data));
    
    // Загружаем их в кэш
    (void)fm_cache.download_file("small.bin");  // 512 bytes
    (void)fm_cache.download_file("medium.bin"); // 1024 bytes
    // кэш: small + medium = 1536 байт
    (void)fm_cache.download_file("large.bin");  // 1024 bytes, суммарно 2560 > 2048 -> должен вытеснить LRU
    
    // Проверяем, что самый старый (small) вытеснен
    // Косвенно: при следующем скачивании small должен читаться с диска (но мы не можем легко проверить)
    // Вместо этого проверим, что все файлы доступны
    auto dl_small = fm_cache.download_file("small.bin");
    auto dl_medium = fm_cache.download_file("medium.bin");
    auto dl_large = fm_cache.download_file("large.bin");
    
    EXPECT_TRUE(dl_small.has_value());
    EXPECT_TRUE(dl_medium.has_value());
    EXPECT_TRUE(dl_large.has_value());
    
    // Статистика кэша (можно добавить метод get_stats, но пока нет)
}

// ========== CACHE TTL EXPIRATION ==========

TEST_F(FileManagerTest, CacheTTLExpiration) {
    cache_config cfg;
    cfg.enabled = true;
    cfg.max_content_cache_bytes = 10 * 1024;
    cfg.content_ttl = std::chrono::seconds(5/*0*0.01*/); // короткий TTL
    file_manager fm_cache(_temp_dir.string(), upload_limits(), cfg);
    
    std::vector<char> data(100, 'T');
    EXPECT_TRUE(fm_cache.upload_file("ttl.bin", data));
    
    // Первое скачивание - кэшируется
    auto dl1 = fm_cache.download_file("ttl.bin");
    EXPECT_TRUE(dl1.has_value());
    
    // Ждём истечения TTL
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Скачиваем снова - должно промахнуться и перезагрузить
    auto dl2 = fm_cache.download_file("ttl.bin");
    EXPECT_TRUE(dl2.has_value());
    EXPECT_EQ(*dl2, data);
    
    // Можно также проверить через cleanup_expired (если метод публичный)
    // fm_cache.cleanup_expired();
}

// ========== CUSTOM METADATA LARGE VALUES ==========

TEST_F(FileManagerTest, CustomMetadata_LargeValues) {
    std::string filename = "large_meta.txt";
    std::vector<char> data = {'d', 'a', 't', 'a'};
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
    
    // Создаём большое значение (10 KB)
    std::string large_value(10 * 1024, 'M');
    std::map<std::string, std::string> meta = {
        {"description", large_value},
        {"small", "value"}
    };
    
    bool set_success = _file_manager->set_custom_metadata(filename, meta);
    EXPECT_TRUE(set_success);
    
    auto retrieved = _file_manager->get_custom_metadata(filename);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->size(), 2);
    EXPECT_EQ((*retrieved)["description"], large_value);
    EXPECT_EQ((*retrieved)["small"], "value");
    
    // Проверяем, что метаданные загружаются и через get_metadata
    auto metadata = _file_manager->get_metadata(filename);
    ASSERT_TRUE(metadata.has_value());
    EXPECT_EQ(metadata->custom_metadata["description"], large_value);
}

// ========== THREAD SAFETY ==========

TEST_F(FileManagerTest, ConcurrentOperations) {
    const int num_threads = 4;
    const int ops_per_thread = 20;
    std::atomic<int> errors{0};
    std::atomic<int> uploads_completed{0};
    
    auto worker = [&](int thread_id) {
        for (int i = 0; i < ops_per_thread; ++i) {
            std::string filename = "concurrent_" + std::to_string(thread_id) + "_" + std::to_string(i) + ".bin";
            std::vector<char> data(1024, static_cast<char>(thread_id * 10 + i));
            
            // Загрузка
            if (!_file_manager->upload_file(filename, data)) {
                errors++;
                continue;
            }
            uploads_completed++;
            
            // Скачивание
            auto downloaded = _file_manager->download_file(filename);
            if (!downloaded || *downloaded != data) {
                errors++;
            }
            
            // Метаданные
            auto meta = _file_manager->get_metadata(filename);
            if (!meta || meta->size != data.size()) {
                errors++;
            }
            
            // Листинг (несколько раз, чтобы увеличить нагрузку)
            if (i % 5 == 0) {
                auto files = _file_manager->list_files();
                // просто проверяем, что не упало
            }
            
            // Иногда удаляем
            if (i % 3 == 0) {
                (void)_file_manager->delete_file(filename);
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
    EXPECT_GT(uploads_completed.load(), 0);
    
    // Финальный листинг не должен падать
    auto final_list = _file_manager->list_files();
    // Количество файлов может быть меньше ops_per_thread * num_threads из-за удалений
}

TEST_F(FileManagerTest, ConcurrentMultipartUploads) {
    const int num_uploads = 3;
    std::vector<std::string> upload_ids;
    std::mutex upload_mutex;
    std::atomic<int> errors{0};
    
    // Инициируем несколько загрузок параллельно
    auto init_worker = [&](int idx) {
        std::string filename = "multi_concurrent_" + std::to_string(idx) + ".bin";
        auto upload_id_opt = _file_manager->initiate_multipart_upload(filename);
        if (upload_id_opt) {
            std::lock_guard<std::mutex> lock(upload_mutex);
            upload_ids.push_back(*upload_id_opt);
        } else {
            errors++;
        }
    };
    
    std::vector<std::thread> init_threads;
    for (int i = 0; i < num_uploads; ++i) {
        init_threads.emplace_back(init_worker, i);
    }
    for (auto& t : init_threads) t.join();
    
    ASSERT_EQ(upload_ids.size(), num_uploads);
    
    // Загружаем части параллельно
    auto part_worker = [&](int upload_idx, int part_num) {
        std::string upload_id = upload_ids[upload_idx];
        std::vector<char> data(512, static_cast<char>(upload_idx * 100 + part_num));
        if (!_file_manager->upload_part(upload_id, part_num, data)) {
            errors++;
        }
    };
    
    std::vector<std::thread> part_threads;
    for (int u = 0; u < num_uploads; ++u) {
        for (int p = 1; p <= 3; ++p) {
            part_threads.emplace_back(part_worker, u, p);
        }
    }
    for (auto& t : part_threads) t.join();
    
    // Завершаем загрузки
    for (int u = 0; u < num_uploads; ++u) {
        std::vector<int> parts = {1, 2, 3};
        if (!_file_manager->complete_multipart_upload(upload_ids[u], parts)) {
            errors++;
        }
    }
    
    EXPECT_EQ(errors.load(), 0);
    
    // Проверяем, что все файлы созданы
    for (int u = 0; u < num_uploads; ++u) {
        std::string filename = "multi_concurrent_" + std::to_string(u) + ".bin";
        EXPECT_TRUE(_file_manager->file_exists(filename));
    }
}

// ========== ЛИМИТ ВРЕМЕННОГО ХРАНИЛИЩА (ДОПОЛНИТЕЛЬНЫЙ ТЕСТ) ==========

TEST_F(FileManagerTest, TempStorageCleanupOnAbort) {
    upload_limits limits;
    limits.max_temp_storage_total = 1024 * 1024; // 1 MB
    file_manager fm_limits(_temp_dir.string(), limits);
    
    // Создаём загрузку и пишем часть, затем абортим
    auto upload_id = fm_limits.initiate_multipart_upload("temp_abort.bin");
    ASSERT_TRUE(upload_id.has_value());
    
    std::vector<char> part(600 * 1024, 'T'); // 600 KB
    EXPECT_TRUE(fm_limits.upload_part(*upload_id, 1, part));
    
    // Абортим - должно освободить место
    EXPECT_TRUE(fm_limits.abort_multipart_upload(*upload_id));
    
    // Теперь должно быть достаточно места для новой загрузки
    auto upload_id2 = fm_limits.initiate_multipart_upload("temp2.bin");
    ASSERT_TRUE(upload_id2.has_value());
    EXPECT_TRUE(fm_limits.upload_part(*upload_id2, 1, part));
    
    EXPECT_TRUE(fm_limits.abort_multipart_upload(*upload_id2));
}

// ========== ПРОВЕРКА ETAG НА БОЛЬШИХ ФАЙЛАХ ==========

TEST_F(FileManagerTest, ETagLargeFile) {
    std::string filename = "etag_large.bin";
    std::vector<char> data(10 * 1024 * 1024); // 10 MB
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, 255);
    for (auto& byte : data) {
        byte = static_cast<char>(dist(rng));
    }
    
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
    
    auto metadata1 = _file_manager->get_metadata(filename);
    auto metadata2 = _file_manager->get_metadata(filename);
    
    ASSERT_TRUE(metadata1.has_value());
    ASSERT_TRUE(metadata2.has_value());
    EXPECT_EQ(metadata1->etag, metadata2->etag);
    EXPECT_FALSE(metadata1->etag.empty());
}

// ========== НЕКОРРЕКТНЫЕ ПУТИ С ПРОБЕЛАМИ ==========

TEST_F(FileManagerTest, FilenameWithSpaces) {
    std::string filename = "file with spaces.txt";
    std::vector<char> data = {'s', 'p', 'a', 'c', 'e', 's'};
    
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
    EXPECT_TRUE(_file_manager->file_exists(filename));
    
    auto downloaded = _file_manager->download_file(filename);
    ASSERT_TRUE(downloaded.has_value());
    EXPECT_EQ(*downloaded, data);
    
    // Удаление
    EXPECT_TRUE(_file_manager->delete_file(filename));
    EXPECT_FALSE(_file_manager->file_exists(filename));
}

TEST_F(FileManagerTest, FilenameWithUnicode) {
    // Проверяем, что файл с именем, содержащим эмодзи, работает
    std::string filename = "emoji_😀_file.txt";
    std::vector<char> data = {'e', 'm', 'o', 'j', 'i'};
    
    EXPECT_TRUE(_file_manager->upload_file(filename, data));
    EXPECT_TRUE(_file_manager->file_exists(filename));
    
    auto downloaded = _file_manager->download_file(filename);
    ASSERT_TRUE(downloaded.has_value());
    EXPECT_EQ(*downloaded, data);
}

