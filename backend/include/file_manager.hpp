#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <cstdint>
#include <map>
#include <mutex>
#include <chrono>
#include <deque>
#include <functional>
#include "cache.hpp"
#include "server.hpp"

namespace fs = std::filesystem;

struct file_metadata {
    std::string name;
    std::uintmax_t size;
    fs::file_time_type last_modified;
    std::string etag;  // MD5 или другой хеш
    std::map<std::string, std::string> custom_metadata; // пользовательские метаданные (x-amz-meta-*)
};

// Метаданные для загрузки по частям
struct multipart_upload {
    std::string upload_id;
    std::string filename;
    fs::path temp_dir;
    std::vector<std::string> parts;
    std::chrono::system_clock::time_point initiated_at;
    bool completed = false;
};

// Метаданные для стриминговой загрузки
struct stream_upload {
    std::string stream_id;
    std::string filename;
    fs::path temp_file;
    std::uintmax_t bytes_written = 0;
    std::chrono::system_clock::time_point initiated_at;
    bool completed = false;
};

struct upload_limits {
    size_t max_file_size = 1024 * 1024 * 1024;      // 1 GB по умолчанию
    size_t max_part_size = 100 * 1024 * 1024;       // 100 MB на часть
    size_t max_parts_per_upload = 10000;            // AWS S3 лимит
    size_t max_temp_storage_total = 10 * 1024 * 1024 * 1024ULL; // 10 GB суммарно для временных файлов
};

class file_manager {
public:
    explicit file_manager(const std::string& storage_path,
                          const upload_limits& limits = upload_limits(),
                          const cache_config& cache_cfg = cache_config());
    ~file_manager();

    // Загрузка файла
    bool upload_file(const std::string& filename, const std::vector<char>& data);
    
    // Скачивание файла
    std::optional<std::vector<char>> download_file(const std::string& filename);
    
    // Удаление файла
    bool delete_file(const std::string& filename);
    
    // Список файлов
    std::vector<file_metadata> list_files();
    
    // Проверка существования файла
    bool file_exists(const std::string& filename) const;
    
    // Получение метаданных
    std::optional<file_metadata> get_metadata(const std::string& filename) const;

    // Установка пользовательских метаданных (x-amz-meta-*)
    bool set_custom_metadata(const std::string& filename, const std::map<std::string, std::string>& metadata);

    // Получение только пользовательских метаданных
    std::optional<std::map<std::string, std::string>> get_custom_metadata(const std::string& filename) const;

    // Инициировать загрузку по частям
    std::optional<std::string> initiate_multipart_upload(const std::string& filename);
    
    // Загрузить часть файла
    bool upload_part(
        const std::string& upload_id,
        int part_number,
        const std::vector<char>& data
    );
    
    // Получить прогресс загрузки
    std::optional<std::map<int, std::uintmax_t>> get_upload_progress(const std::string& upload_id) const;
    
    // Завершить загрузку (объединить части)
    bool complete_multipart_upload(
        const std::string& upload_id,
        const std::vector<int>& part_numbers
    );
    
    // Отменить загрузку
    bool abort_multipart_upload(const std::string& upload_id);
    
    // Очистить старые загрузки (старше указанного времени)
    void cleanup_old_uploads(std::chrono::hours max_age = std::chrono::hours(24));

    // ========== СТРИМИНГОВАЯ ЗАГРУЗКА ==========
    
    // Инициировать стриминговую загрузку файла
    std::optional<std::string> initiate_stream_upload(const std::string& filename);
    
    // Записать данные в стрим
    bool write_to_stream(
        const std::string& stream_id,
        const std::vector<char>& data
    );
    
    // Завершить стриминговую загрузку
    bool complete_stream_upload(const std::string& stream_id);
    
    // Отменить стриминговую загрузку
    bool abort_stream_upload(const std::string& stream_id);
    
    // Получить прогресс стриминговой загрузки (размер записанных данных)
    std::optional<std::uintmax_t> get_stream_upload_progress(const std::string& stream_id) const;

    fs::path get_full_path(const std::string& filename) const;

    bool upload_file_stream(const std::string& filename, std::istream& data);
    bool download_file_stream(const std::string& filename, std::ostream& out);
    
    // Метод для получения FILE* или дескриптора для sendfile
    int get_file_descriptor(const std::string& filename) const; // или возвращать FILE*
    

private:
    fs::path _storage_dir;
    fs::path _storage_dir_canonical; // закэшированный канонический путь
    mutable std::mutex _mutex;
    std::map<std::string, multipart_upload> _active_uploads;
    std::map<std::string, stream_upload> _active_stream_uploads;

    upload_limits _limits;
    cache_config _cache_cfg;
    mutable file_content_cache _content_cache;
    mutable metadata_cache _metadata_cache;
    // Функция для подсчёта текущего объёма временных файлов
    uint64_t get_current_temp_storage_usage() const;

    // Cache management
    void invalidate_content_cache(const std::string& filename);
    void invalidate_metadata_cache(const std::string& filename);
    void invalidate_cache_for_file(const std::string& filename);
    
   // Вычисление ETag из данных
    std::string compute_etag(const std::vector<char>& data) const;
    std::string compute_etag_MD5(const std::vector<char>& data) const;
    std::string compute_etag_SHA256(const std::vector<char>& data) const;    
    // Вычисление ETag из файла
    std::string compute_etag(const fs::path& file_path) const;
    
    // Проверка безопасности пути (защита от path traversal)
    bool is_path_safe(const std::string& filename) const;

    // Генерация уникального ID для загрузки
    std::string generate_upload_id() const;
    
    // Получение метаданных загрузки
    std::optional<std::reference_wrapper<multipart_upload>> get_upload(const std::string& upload_id);
    std::optional<std::reference_wrapper<const multipart_upload>> get_upload(const std::string& upload_id) const;
    
    // Объединение частей в один файл
    bool merge_parts(const multipart_upload& upload, const std::vector<int>& part_numbers);
    
    // Удаление временных файлов загрузки
    void cleanup_upload(const std::string& upload_id);
    
    // ========== СТРИМИНГОВАЯ ЗАГРУЗКА (приватные методы) ==========
    
    // Получение стриминговой загрузки
    std::optional<std::reference_wrapper<stream_upload>> get_stream_upload(const std::string& stream_id);
    std::optional<std::reference_wrapper<const stream_upload>> get_stream_upload(const std::string& stream_id) const;
    
    // Очистка стриминговой загрузки
    void cleanup_stream_upload(const std::string& stream_id);
    
    // ========== МЕТАДАННЫЕ ==========
    
    // Загрузка пользовательских метаданных из файла
    std::optional<std::map<std::string, std::string>> load_custom_metadata(const std::string& filename) const;
    
    // Сохранение пользовательских метаданных в файл
    bool save_custom_metadata(const std::string& filename, const std::map<std::string, std::string>& metadata) const;
    
    // Получение пути к файлу метаданных
    fs::path get_metadata_file_path(const std::string& filename) const;
    
    // Вспомогательные методы для копирования с буфером
    bool copy_with_buffer(std::ifstream& src, std::ofstream& dst, size_t buffer_size = 1024 * 1024);
    bool copy_with_sendfile(int src_fd, int dst_fd, off_t size);
};