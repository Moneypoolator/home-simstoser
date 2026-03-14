#include "file_manager.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <random>
#include <algorithm>
#include <glog/logging.h>
#include "logging.hpp"

file_manager::file_manager(const std::string& storage_path)
    : _storage_dir(storage_path)
{
    // Создаем директорию для хранения, если она не существует
    fs::create_directories(_storage_dir);
    LOG(INFO) << "File manager initialized. Storage path: " << _storage_dir.string();
}

file_manager::~file_manager()
{
    // Очищаем все активные загрузки при уничтожении
    std::lock_guard<std::mutex> lock(_mutex);
    for (const auto& [upload_id, upload] : _active_uploads) {
        if (!upload.completed) {
            fs::remove_all(upload.temp_dir);
        }
    }
    VLOG(1) << "File manager destroyed";
}


bool file_manager::upload_file(const std::string& filename, const std::vector<char>& data)
{
    if (!is_path_safe(filename)) {
        LOG(WARNING) << "Attempted unsafe path access: " << filename;
        return false;
    }

    try {
        fs::path file_path = _storage_dir / filename;
        
        // Создаем родительские директории, если нужно
        fs::create_directories(file_path.parent_path());
        
        // Записываем файл
        std::ofstream file(file_path, std::ios::binary | std::ios::trunc);
        if (!file) {
            LOG(ERROR) << "Failed to open file for writing: " << file_path.string();
            return false;
        }
        
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        file.close();
        
        if (file.good()) {
            logging::log_file_operation("UPLOAD", filename, data.size());
            return true;
        } else {
            LOG(ERROR) << "Failed to write file: " << filename;
            return false;
        }
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception during file upload: " << e.what();
        return false;
    }
}

std::optional<std::vector<char>> file_manager::download_file(const std::string& filename)
{
    if (!is_path_safe(filename)) {
        LOG(WARNING) << "Attempted unsafe path access: " << filename;
        return std::nullopt;
    }

    try {
        fs::path file_path = _storage_dir / filename;
        
        if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
            VLOG(1) << "File not found: " << filename;
            return std::nullopt;
        }
        
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file) {
            LOG(ERROR) << "Failed to open file for reading: " << file_path.string();
            return std::nullopt;
        }
        
        auto file_size = file.tellg();
        std::vector<char> data(file_size);
        
        file.seekg(0, std::ios::beg);
        file.read(data.data(), file_size);
        file.close();
        
        logging::log_file_operation("DOWNLOAD", filename, file_size);
        return data;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception during file download: " << e.what();
        return std::nullopt;
    }
}

bool file_manager::delete_file(const std::string& filename)
{
    if (!is_path_safe(filename)) {
        return false;
    }

    try {
        fs::path file_path = _storage_dir / filename;
        
        if (!fs::exists(file_path)) {
            return false;
        }
        
        return fs::remove(file_path);
    } catch (const std::exception& e) {
        return false;
    }
}

std::vector<file_metadata> file_manager::list_files()
{
    std::vector<file_metadata> result;
    
    try {
        if (!fs::exists(_storage_dir) || !fs::is_directory(_storage_dir)) {
            return result;
        }
        
        // Рекурсивно обходим все файлы
        for (const auto& entry : fs::recursive_directory_iterator(_storage_dir)) {
            if (fs::is_regular_file(entry.status())) {
                file_metadata meta;
                meta.name = fs::relative(entry.path(), _storage_dir).string();
                meta.size = fs::file_size(entry.path());
                meta.last_modified = fs::last_write_time(entry.path());
                meta.etag = compute_etag(entry.path());
                result.push_back(meta);
            }
        }
    } catch (const std::exception& e) {
        // В случае ошибки возвращаем пустой список
        result.clear();
    }
    
    return result;
}

bool file_manager::file_exists(const std::string& filename) const
{
    if (!is_path_safe(filename)) {
        return false;
    }

    try {
        fs::path file_path = _storage_dir / filename;
        return fs::exists(file_path) && fs::is_regular_file(file_path);
    } catch (const std::exception& e) {
        return false;
    }
}

std::optional<file_metadata> file_manager::get_metadata(const std::string& filename) const
{
    if (!is_path_safe(filename)) {
        return std::nullopt;
    }

    try {
        fs::path file_path = _storage_dir / filename;
        
        if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
            return std::nullopt;
        }
        
        file_metadata meta;
        meta.name = filename;
        meta.size = fs::file_size(file_path);
        meta.last_modified = fs::last_write_time(file_path);
        meta.etag = compute_etag(file_path);
        
        return meta;
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

std::optional<std::string> file_manager::initiate_multipart_upload(const std::string& filename)
{
    if (!is_path_safe(filename)) {
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    
    // Генерируем уникальный ID загрузки
    std::string upload_id = generate_upload_id();
    
    // Создаем временную директорию для частей
    fs::path temp_dir = _storage_dir / ".uploads" / upload_id;
    fs::create_directories(temp_dir);
    
    // Создаем запись о загрузке
    multipart_upload upload;
    upload.upload_id = upload_id;
    upload.filename = filename;
    upload.temp_dir = temp_dir;
    upload.initiated_at = std::chrono::system_clock::now();
    upload.completed = false;
    
    _active_uploads[upload_id] = std::move(upload);
    
    return upload_id;
}

bool file_manager::upload_part(
    const std::string& upload_id,
    int part_number,
    const std::vector<char>& data)
{
    if (data.empty() || part_number <= 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    
    auto upload_opt = get_upload(upload_id);
    if (!upload_opt || (*upload_opt)->completed) {
        return false;
    }
    
    multipart_upload* upload = *upload_opt;
    
    // Формируем имя файла для части
    std::ostringstream part_name;
    part_name << "part_" << std::setw(5) << std::setfill('0') << part_number;
    std::string part_filename = part_name.str();
    
    // Путь к файлу части
    fs::path part_path = upload->temp_dir / part_filename;
    
    // Сохраняем часть
    std::ofstream part_file(part_path, std::ios::binary | std::ios::trunc);
    if (!part_file) {
        return false;
    }
    
    part_file.write(data.data(), static_cast<std::streamsize>(data.size()));
    part_file.close();
    
    if (!part_file.good()) {
        return false;
    }
    
    // Добавляем часть в список (если ещё не добавлена)
    if (std::find(upload->parts.begin(), upload->parts.end(), part_filename) == upload->parts.end()) {
        upload->parts.push_back(part_filename);
    }
    
    return true;
}

std::optional<std::map<int, std::uintmax_t>> file_manager::get_upload_progress(const std::string& upload_id) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto upload_opt = get_upload(upload_id);
    if (!upload_opt) {
        return std::nullopt;
    }
    
    const multipart_upload* upload = *upload_opt;
    
    std::map<int, std::uintmax_t> progress;
    
    for (const auto& part_name : upload->parts) {
        // Извлекаем номер части из имени файла
        try {
            int part_number = std::stoi(part_name.substr(5)); // "part_XXXXX"
            fs::path part_path = upload->temp_dir / part_name;
            
            if (fs::exists(part_path)) {
                progress[part_number] = fs::file_size(part_path);
            }
        } catch (...) {
            continue;
        }
    }
    
    return progress;
}

bool file_manager::complete_multipart_upload(
    const std::string& upload_id,
    const std::vector<int>& part_numbers)
{
    if (part_numbers.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    
    auto upload_opt = get_upload(upload_id);
    if (!upload_opt || (*upload_opt)->completed) {
        return false;
    }
    
    multipart_upload* upload = *upload_opt;
    
    // Объединяем части
    if (!merge_parts(*upload, part_numbers)) {
        return false;
    }
    
    // Помечаем как завершённую
    upload->completed = true;
    
    // Очищаем временные файлы
    cleanup_upload(upload_id);
    
    return true;
}

bool file_manager::abort_multipart_upload(const std::string& upload_id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto upload_opt = get_upload(upload_id);
    if (!upload_opt) {
        return false;
    }
    
    // Удаляем временные файлы
    cleanup_upload(upload_id);
    
    return true;
}

void file_manager::cleanup_old_uploads(std::chrono::hours max_age)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto now = std::chrono::system_clock::now();
    std::vector<std::string> uploads_to_remove;
    
    for (const auto& [upload_id, upload] : _active_uploads) {
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - upload.initiated_at);
        
        if (age > max_age && !upload.completed) {
            uploads_to_remove.push_back(upload_id);
        }
    }
    
    // Удаляем старые загрузки
    for (const auto& upload_id : uploads_to_remove) {
        cleanup_upload(upload_id);
        _active_uploads.erase(upload_id);
    }
}

std::string file_manager::compute_etag_MD5(const std::vector<char>& data) const
{
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest);
    
    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    
    return ss.str();
}

std::string file_manager::compute_etag_SHA256(const std::vector<char>& data) const
{
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    
    return ss.str();
}

std::string file_manager::compute_etag(const std::vector<char>& data) const
{
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) {
        return "";
    }
    
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    if (EVP_DigestUpdate(ctx, data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;
    
    if (EVP_DigestFinal_ex(ctx, digest, &digest_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return "";
    }
    
    EVP_MD_CTX_free(ctx);
    
    std::stringstream ss;
    for (unsigned int i = 0; i < digest_len; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    
    return ss.str();
}

std::string file_manager::compute_etag(const fs::path& file_path) const
{
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file) {
        return "";
    }
    
    auto file_size = file.tellg();
    std::vector<char> data(file_size);
    
    file.seekg(0, std::ios::beg);
    file.read(data.data(), file_size);
    
    return compute_etag(data);
}


bool file_manager::is_path_safe(const fs::path& storage_dir, const std::string& filename) 
{
    // Проверка на пустое имя
    if (filename.empty()) {
        return false;
    }
    
    // Проверка на попытку выхода за пределы хранилища (path traversal)
    fs::path path(filename);
    
    // Проверяем, что путь не содержит ".."
    for (const auto& part : path) {
        if (part == ".." || part == "/") {
            return false;
        }
    }
    
    // Проверяем, что путь не абсолютный
    if (path.is_absolute()) {
        return false;
    }
    
    // Проверяем, что результирующий путь остается внутри _storage_dir
    fs::path resolved = fs::weakly_canonical(storage_dir / path);
    fs::path base = fs::weakly_canonical(storage_dir);
    
    // Проверяем, что resolved начинается с base
    return resolved.string().substr(0, base.string().length()) == base.string();
}

bool file_manager::is_path_safe(const std::string& filename) const
{
    return is_path_safe(_storage_dir, filename);
}

std::string file_manager::generate_upload_id() const
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << std::hex;
    
    // Генерируем 32-символьный hex ID (как UUID без дефисов)
    for (int i = 0; i < 32; ++i) {
        ss << dis(gen);
    }
    
    return ss.str();
}

std::optional<multipart_upload*> file_manager::get_upload(const std::string& upload_id)
{
    auto it = _active_uploads.find(upload_id);
    if (it == _active_uploads.end()) {
        return std::nullopt;
    }
    return &it->second;
}

std::optional<const multipart_upload*> file_manager::get_upload(const std::string& upload_id) const
{
    auto it = _active_uploads.find(upload_id);
    if (it == _active_uploads.end()) {
        return std::nullopt;
    }
    return &it->second;
}

bool file_manager::merge_parts(const multipart_upload& upload, const std::vector<int>& part_numbers)
{
    try {
        // Путь к финальному файлу
        fs::path final_path = _storage_dir / upload.filename;
        fs::create_directories(final_path.parent_path());
        
        // Открываем файл для записи
        std::ofstream final_file(final_path, std::ios::binary | std::ios::trunc);
        if (!final_file) {
            return false;
        }
        
        // Объединяем все части в правильном порядке
        for (int part_number : part_numbers) {
            std::ostringstream part_name;
            part_name << "part_" << std::setw(5) << std::setfill('0') << part_number;
            
            fs::path part_path = upload.temp_dir / part_name.str();
            
            if (!fs::exists(part_path)) {
                final_file.close();
                return false;
            }
            
            // Читаем часть
            std::ifstream part_file(part_path, std::ios::binary | std::ios::ate);
            if (!part_file) {
                final_file.close();
                return false;
            }
            
            auto part_size = part_file.tellg();
            std::vector<char> part_data(part_size);
            
            part_file.seekg(0, std::ios::beg);
            part_file.read(part_data.data(), part_size);
            part_file.close();
            
            // Записываем в финальный файл
            final_file.write(part_data.data(), static_cast<std::streamsize>(part_data.size()));
        }
        
        final_file.close();
        
        return final_file.good();
    } catch (const std::exception& e) {
        return false;
    }
}

void file_manager::cleanup_upload(const std::string& upload_id)
{
    auto upload_opt = get_upload(upload_id);
    if (!upload_opt) {
        return;
    }
    
    const multipart_upload* upload = *upload_opt;
    
    // Удаляем временную директорию
    try {
        if (fs::exists(upload->temp_dir)) {
            fs::remove_all(upload->temp_dir);
        }
    } catch (...) {
        // Игнорируем ошибки при удалении
    }
    
    // Удаляем из активных загрузок
    _active_uploads.erase(upload_id);
}