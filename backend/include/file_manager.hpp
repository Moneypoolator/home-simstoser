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

namespace fs = std::filesystem;

struct file_metadata {
    std::string name;
    std::uintmax_t size;
    fs::file_time_type last_modified;
    std::string etag;  // MD5 или другой хеш
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

class file_manager {
public:
    explicit file_manager(const std::string& storage_path);
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

    static bool is_path_safe(const fs::path& storage_dir, const std::string& filename);

private:
    fs::path _storage_dir;
    mutable std::mutex _mutex;
    std::map<std::string, multipart_upload> _active_uploads;
    
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
    std::optional<multipart_upload*> get_upload(const std::string& upload_id);
    std::optional<const multipart_upload*> get_upload(const std::string& upload_id) const;
    
    // Объединение частей в один файл
    bool merge_parts(const multipart_upload& upload, const std::vector<int>& part_numbers);
    
    // Удаление временных файлов загрузки
    void cleanup_upload(const std::string& upload_id);
};