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

#include "path_utils.hpp"
#include "logging.hpp"


file_manager::file_manager(const std::string& storage_path)
    : _storage_dir(storage_path)
{
    // Создаем директорию для хранения, если она не существует
    fs::create_directories(_storage_dir);
    // Вычисляем канонический путь один раз при инициализации
    try {
        _storage_dir_canonical = fs::canonical(_storage_dir);
    } catch (const std::exception& e) {
        LOG(WARNING) << "Cannot resolve canonical path for storage dir: " << e.what();
        _storage_dir_canonical = fs::weakly_canonical(_storage_dir);
    }
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


bool file_manager::upload_file(const std::string& filename, const std::vector<char>& data) {
    // Превращаем вектор в поток
    std::stringstream ss;
    ss.write(data.data(), data.size());
    return upload_file_stream(filename, ss);
}

std::optional<std::vector<char>> file_manager::download_file(const std::string& filename) {
    std::stringstream ss;
    if (!download_file_stream(filename, ss)) {
        return std::nullopt;
    }
    std::string str = ss.str();
    return std::vector<char>(str.begin(), str.end());
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
        LOG(ERROR) << "Exception during file deletion: " << e.what();
        return false;
    }
}

std::vector<file_metadata> file_manager::list_files()
{
    std::vector<file_metadata> result;
    
    try {
        if (!fs::exists(_storage_dir) || !fs::is_directory(_storage_dir)) {
            LOG(WARNING) << "Storage directory not found: " << _storage_dir.string();
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
        
        VLOG(1) << "Listed " << result.size() << " files";
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception during file listing: " << e.what();
        result.clear();
    }
    
    return result;
}

bool file_manager::file_exists(const std::string& filename) const
{
    if (!is_path_safe(filename)) {
        LOG(WARNING) << "Attempted unsafe path access: " << filename;
        return false;
    }

    try {
        fs::path file_path = _storage_dir / filename;
        bool exists = fs::exists(file_path) && fs::is_regular_file(file_path);
        
        if (!exists) {
            VLOG(2) << "File existence check: " << filename << " - not found";
        } else {
            VLOG(2) << "File existence check: " << filename << " - exists";
        }
        
        return exists;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception checking file existence: " << filename << " - " << e.what();
        return false;
    }
}

std::optional<file_metadata> file_manager::get_metadata(const std::string& filename) const
{
    if (!is_path_safe(filename)) {
        LOG(WARNING) << "Attempted unsafe path access: " << filename;
        return std::nullopt;
    }

    try {
        fs::path file_path = _storage_dir / filename;
        
        if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
            VLOG(2) << "Metadata request: file not found - " << filename;
            return std::nullopt;
        }
        
        file_metadata meta;
        meta.name = filename;
        meta.size = fs::file_size(file_path);
        meta.last_modified = fs::last_write_time(file_path);
        meta.etag = compute_etag(file_path);
        
        VLOG(2) << "Metadata retrieved for: " << filename << " (" << meta.size << " bytes)";
        
        return meta;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception getting metadata: " << filename << " - " << e.what();
        return std::nullopt;
    }
}

std::optional<std::string> file_manager::initiate_multipart_upload(const std::string& filename)
{
    if (!is_path_safe(filename)) {
        LOG(WARNING) << "Attempted unsafe path access: " << filename;
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
    
    LOG(INFO) << "Multipart upload initiated: " << filename << " (ID: " << upload_id << ")";
    
    return upload_id;
}

bool file_manager::upload_part(
    const std::string& upload_id,
    int part_number,
    const std::vector<char>& data)
{
    if (data.empty() || part_number <= 0) {
        LOG(WARNING) << "Invalid part upload parameters: upload_id=" << upload_id 
                     << ", part=" << part_number << ", size=" << data.size();
        return false;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    
    auto upload_opt = get_upload(upload_id);
    if (!upload_opt || (*upload_opt)->completed) {
        LOG(WARNING) << "Upload not found or already completed: " << upload_id;
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
        LOG(ERROR) << "Failed to open part file for writing: " << part_path.string();
        return false;
    }
    
    part_file.write(data.data(), static_cast<std::streamsize>(data.size()));
    part_file.close();
    
    if (!part_file.good()) {
        LOG(ERROR) << "Failed to write part file: " << part_path.string();
        return false;
    }
    
    // Добавляем часть в список (если ещё не добавлена)
    if (std::find(upload->parts.begin(), upload->parts.end(), part_filename) == upload->parts.end()) {
        upload->parts.push_back(part_filename);
    }
    
    LOG(INFO) << "Part uploaded: upload_id=" << upload_id 
              << ", part=" << part_number << ", size=" << data.size() << " bytes";
    
    return true;
}

std::optional<std::map<int, std::uintmax_t>> file_manager::get_upload_progress(const std::string& upload_id) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto upload_opt = get_upload(upload_id);
    if (!upload_opt) {
        VLOG(1) << "Progress requested for non-existent upload: " << upload_id;
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
    
    VLOG(1) << "Upload progress retrieved: upload_id=" << upload_id 
            << ", parts=" << progress.size();
    
    return progress;
}

bool file_manager::complete_multipart_upload(
    const std::string& upload_id,
    const std::vector<int>& part_numbers)
{
    if (part_numbers.empty()) {
        LOG(WARNING) << "Empty part numbers list for upload completion: " << upload_id;
        return false;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    
    auto upload_opt = get_upload(upload_id);
    if (!upload_opt || (*upload_opt)->completed) {
        LOG(WARNING) << "Upload not found or already completed: " << upload_id;
        return false;
    }
    
    multipart_upload* upload = *upload_opt;
    
    // Объединяем части
    if (!merge_parts(*upload, part_numbers)) {
        LOG(ERROR) << "Failed to merge parts for upload: " << upload_id;
        return false;
    }
    
    // Помечаем как завершённую
    upload->completed = true;
    
    // Очищаем временные файлы
    cleanup_upload(upload_id);
    
    LOG(INFO) << "Multipart upload completed: " << upload_id 
              << " (" << upload->filename << ")";
    
    return true;
}

bool file_manager::abort_multipart_upload(const std::string& upload_id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto upload_opt = get_upload(upload_id);
    if (!upload_opt) {
        VLOG(1) << "Attempted to abort non-existent upload: " << upload_id;
        return false;
    }
    
    // Удаляем временные файлы
    cleanup_upload(upload_id);
    
    LOG(INFO) << "Multipart upload aborted: " << upload_id;
    
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
    
    if (!uploads_to_remove.empty()) {
	    LOG(INFO) << "Cleaned up " << uploads_to_remove.size() << " old uploads";
	}
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

bool file_manager::is_path_safe(const std::string& filename) const {
    return path_utils::is_path_safe(_storage_dir, filename);
}

fs::path file_manager::get_full_path(const std::string& filename) const {
    if (!is_path_safe(filename)) {
        throw std::runtime_error("Unsafe path: " + filename);
    }
    return _storage_dir / filename;
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

bool file_manager::merge_parts(const multipart_upload& upload, const std::vector<int>& part_numbers) {
    try {
        fs::path final_path = _storage_dir / upload.filename;
        fs::create_directories(final_path.parent_path());
        
        std::ofstream final_file(final_path, std::ios::binary | std::ios::trunc);
        if (!final_file) {
            LOG(ERROR) << "Cannot create final file: " << final_path;
            return false;
        }
        
        constexpr size_t buffer_size = 1024 * 1024; // 1 MB
        std::vector<char> buffer(buffer_size);
        
        for (int part_number : part_numbers) {
            std::ostringstream part_name;
            part_name << "part_" << std::setw(5) << std::setfill('0') << part_number;
            fs::path part_path = upload.temp_dir / part_name.str();
            
            if (!fs::exists(part_path)) {
                LOG(ERROR) << "Missing part: " << part_path;
                return false;
            }
            
            std::ifstream part_file(part_path, std::ios::binary);
            if (!part_file) {
                LOG(ERROR) << "Cannot open part: " << part_path;
                return false;
            }
            
            // Копируем с буфером
            while (part_file) {
                part_file.read(buffer.data(), buffer_size);
                std::streamsize bytes = part_file.gcount();
                if (bytes > 0) {
                    final_file.write(buffer.data(), bytes);
                    if (!final_file) {
                        LOG(ERROR) << "Write error while merging part " << part_number;
                        return false;
                    }
                }
            }
        }
        
        final_file.close();
        LOG(INFO) << "Merged " << part_numbers.size() << " parts into " << upload.filename;
        return true;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Merge parts exception: " << e.what();
        return false;
    }
}

void file_manager::cleanup_upload(const std::string& upload_id)
{
    auto upload_opt = get_upload(upload_id);
    if (!upload_opt) {
        VLOG(2) << "Cleanup requested for non-existent upload: " << upload_id;
        return;
    }
    
    const multipart_upload* upload = *upload_opt;
    
    // Удаляем временную директорию
    try {
        if (fs::exists(upload->temp_dir)) {
            fs::remove_all(upload->temp_dir);
            VLOG(2) << "Upload cleanup completed: " << upload_id;
        }
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception during upload cleanup: " << upload_id << " - " << e.what();
    }
    
    // Удаляем из активных загрузок
    _active_uploads.erase(upload_id);
}

// ========== СТРИМИНГОВАЯ ЗАГРУЗКА ==========

std::optional<std::string> file_manager::initiate_stream_upload(const std::string& filename)
{
    if (!is_path_safe(filename)) {
        LOG(WARNING) << "Attempted unsafe path access: " << filename;
        return std::nullopt;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    
    // Генерируем уникальный ID загрузки
    std::string stream_id = generate_upload_id();
    
    // Создаем временный файл для записи
    fs::path temp_dir = _storage_dir / ".stream_uploads";
    fs::create_directories(temp_dir);
    
    fs::path temp_file = temp_dir / stream_id;
    
    // Создаем пустой временный файл
    try {
        std::ofstream empty_file(temp_file, std::ios::binary | std::ios::trunc);
        if (!empty_file) {
            LOG(ERROR) << "Failed to create temporary file for stream: " << temp_file.string();
            return std::nullopt;
        }
        empty_file.close();
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception creating temporary file: " << e.what();
        return std::nullopt;
    }
    
    // Создаем запись о стриминговой загрузке
    stream_upload upload;
    upload.stream_id = stream_id;
    upload.filename = filename;
    upload.temp_file = temp_file;
    upload.bytes_written = 0;
    upload.initiated_at = std::chrono::system_clock::now();
    upload.completed = false;
    
    _active_stream_uploads[stream_id] = std::move(upload);
    
    LOG(INFO) << "Stream upload initiated: " << filename << " (ID: " << stream_id << ")";
    return stream_id;
}

bool file_manager::write_to_stream(
    const std::string& stream_id,
    const std::vector<char>& data)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto upload_it = _active_stream_uploads.find(stream_id);
    if (upload_it == _active_stream_uploads.end()) {
        LOG(WARNING) << "Stream upload not found: " << stream_id;
        return false;
    }
    
    stream_upload& upload = upload_it->second;
    
    if (upload.completed) {
        LOG(WARNING) << "Attempt to write to completed stream: " << stream_id;
        return false;
    }
    
    try {
        // Открываем файл в режиме добавления (append)
        std::ofstream file(upload.temp_file, std::ios::binary | std::ios::app);
        if (!file) {
            LOG(ERROR) << "Failed to open stream file for writing: " << upload.temp_file.string();
            return false;
        }
        
        file.write(data.data(), static_cast<std::streamsize>(data.size()));
        file.close();
        
        if (file.good()) {
            upload.bytes_written += data.size();
            VLOG(2) << "Written " << data.size() << " bytes to stream " << stream_id
                    << " (total: " << upload.bytes_written << " bytes)";
            return true;
        } else {
            LOG(ERROR) << "Failed to write to stream: " << stream_id;
            return false;
        }
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception during stream write: " << stream_id << " - " << e.what();
        return false;
    }
}

bool file_manager::complete_stream_upload(const std::string& stream_id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto upload_it = _active_stream_uploads.find(stream_id);
    if (upload_it == _active_stream_uploads.end()) {
        LOG(WARNING) << "Stream upload not found: " << stream_id;
        return false;
    }
    
    stream_upload& upload = upload_it->second;
    
    if (upload.completed) {
        LOG(WARNING) << "Stream upload already completed: " << stream_id;
        return false;
    }
    
    if (!is_path_safe(upload.filename)) {
        LOG(WARNING) << "Unsafe filename in stream upload: " << upload.filename;
        return false;
    }
    
    try {
        fs::path final_path = _storage_dir / upload.filename;
        
        // Создаем родительские директории, если нужно
        fs::create_directories(final_path.parent_path());
        
        // Перемещаем временный файл в финальное расположение
        fs::rename(upload.temp_file, final_path);
        
        upload.completed = true;
        
        LOG(INFO) << "Stream upload completed: " << upload.filename
                  << " (ID: " << stream_id << ", size: " << upload.bytes_written << " bytes)";
        logging::log_file_operation("STREAM_UPLOAD", upload.filename, upload.bytes_written);
        
        // Удаляем из активных загрузок
        _active_stream_uploads.erase(upload_it);
        
        return true;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception during stream completion: " << stream_id << " - " << e.what();
        return false;
    }
}

bool file_manager::abort_stream_upload(const std::string& stream_id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto upload_it = _active_stream_uploads.find(stream_id);
    if (upload_it == _active_stream_uploads.end()) {
        VLOG(2) << "Stream upload not found for abort: " << stream_id;
        return false;
    }
    
    stream_upload& upload = upload_it->second;
    
    try {
        // Удаляем временный файл
        if (fs::exists(upload.temp_file)) {
            fs::remove(upload.temp_file);
        }
        
        LOG(INFO) << "Stream upload aborted: " << upload.filename
                  << " (ID: " << stream_id << ", written: " << upload.bytes_written << " bytes)";
        
        // Удаляем из активных загрузок
        _active_stream_uploads.erase(upload_it);
        
        return true;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception during stream abort: " << stream_id << " - " << e.what();
        return false;
    }
}

std::optional<std::uintmax_t> file_manager::get_stream_upload_progress(const std::string& stream_id) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    
    auto upload_it = _active_stream_uploads.find(stream_id);
    if (upload_it == _active_stream_uploads.end()) {
        VLOG(2) << "Stream upload not found for progress: " << stream_id;
        return std::nullopt;
    }
    
    const stream_upload& upload = upload_it->second;
    return upload.bytes_written;
}

std::optional<stream_upload*> file_manager::get_stream_upload(const std::string& stream_id)
{
    auto it = _active_stream_uploads.find(stream_id);
    if (it == _active_stream_uploads.end()) {
        return std::nullopt;
    }
    return &it->second;
}

std::optional<const stream_upload*> file_manager::get_stream_upload(const std::string& stream_id) const
{
    auto it = _active_stream_uploads.find(stream_id);
    if (it == _active_stream_uploads.end()) {
        return std::nullopt;
    }
    return &it->second;
}

void file_manager::cleanup_stream_upload(const std::string& stream_id)
{
    auto upload_opt = get_stream_upload(stream_id);
    if (!upload_opt) {
        VLOG(2) << "Cleanup requested for non-existent stream upload: " << stream_id;
        return;
    }
    
    const stream_upload* upload = *upload_opt;
    
    // Удаляем временный файл
    try {
        if (fs::exists(upload->temp_file)) {
            fs::remove(upload->temp_file);
            VLOG(2) << "Stream upload cleanup completed: " << stream_id;
        }
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception during stream upload cleanup: " << stream_id << " - " << e.what();
    }
    
    // Удаляем из активных загрузок
    _active_stream_uploads.erase(stream_id);
}

bool file_manager::upload_file_stream(const std::string& filename, std::istream& data) {
    if (!is_path_safe(filename)) {
        LOG(WARNING) << "Unsafe path: " << filename;
        return false;
    }
    
    try {
        fs::path file_path = _storage_dir / filename;
        fs::create_directories(file_path.parent_path());
        
        std::ofstream file(file_path, std::ios::binary | std::ios::trunc);
        if (!file) {
            LOG(ERROR) << "Cannot create file: " << file_path;
            return false;
        }
        
        // Буфер 1 MB
        constexpr size_t buffer_size = 1024 * 1024;
        std::vector<char> buffer(buffer_size);
        
        while (data) {
            data.read(buffer.data(), buffer_size);
            std::streamsize bytes = data.gcount();
            if (bytes > 0) {
                file.write(buffer.data(), bytes);
                if (!file) {
                    LOG(ERROR) << "Write error during upload: " << filename;
                    return false;
                }
            }
        }
        
        file.close();
        logging::log_file_operation("UPLOAD_STREAM", filename, fs::file_size(file_path));
        return true;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception in upload_file_stream: " << e.what();
        return false;
    }
}



#ifdef __linux__
#include <sys/sendfile.h>
#include <fcntl.h>
#include <unistd.h>

bool file_manager::download_file_stream(const std::string& filename, std::ostream& out) {
    if (!is_path_safe(filename)) {
        LOG(WARNING) << "Unsafe path: " << filename;
        return false;
    }
    
    fs::path file_path = _storage_dir / filename;
    if (!fs::exists(file_path) || !fs::is_regular_file(file_path)) {
        return false;
    }
    
    int fd = open(file_path.c_str(), O_RDONLY);
    if (fd == -1) {
        LOG(ERROR) << "Cannot open file for sendfile: " << filename;
        return false;
    }
    
    off_t size = fs::file_size(file_path);
    off_t offset = 0;
    
    // Если out — это сокет, можно использовать sendfile напрямую, но у нас ostream.
    // Для ostream мы не можем использовать sendfile, поэтому делаем буферизированное чтение.
    // В HTTP-ответе мы можем использовать beast::http::response с body-генератором,
    // но для простоты здесь реализуем буфер.
    
    constexpr size_t buffer_size = 1024 * 1024;
    std::vector<char> buffer(buffer_size);
    
    ssize_t bytes_sent;
    while (offset < size) {
        size_t to_read = std::min(buffer_size, static_cast<size_t>(size - offset));
        ssize_t bytes_read = read(fd, buffer.data(), to_read);
        if (bytes_read <= 0) break;
        out.write(buffer.data(), bytes_read);
        offset += bytes_read;
    }
    
    close(fd);
    return offset == size;
}

#else
// Windows: использовать ReadFile / WriteFile или TransmitFile
#endif

