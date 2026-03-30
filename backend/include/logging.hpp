#pragma once
#include <glog/logging.h>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;
namespace logging {

// Инициализация библиотеки glog
inline void init(const std::string& program_name = "s3_server", 
                 bool log_to_stderr = true,
                 bool color_log = true,
                 int verbosity = 1) {
    // Инициализируем Google Logging
    google::InitGoogleLogging(program_name.c_str());
    
    // Настройки логирования
    FLAGS_logtostderr = log_to_stderr;        // Логировать в stderr
    FLAGS_colorlogtostderr = color_log;       // Цветное логирование
    FLAGS_v = verbosity;                       // Уровень детализации (0-3)
    FLAGS_log_dir = "logs";                    // Директория для логов
    FLAGS_max_log_size = 100;                  // Максимальный размер файла лога (МБ)
    FLAGS_stop_logging_if_full_disk = true;   // Остановить логирование при нехватке места
    
    // Создаем директорию для логов с помощью std::filesystem
    fs::create_directories(FLAGS_log_dir);
    
    LOG(INFO) << "Logging initialized for " << program_name;
}

// Завершение работы с логированием
inline void shutdown() {
    google::ShutdownGoogleLogging();
}

// Макросы для удобного использования
// Логирование с указанием файла и строки
#define LOG_DEBUG(msg) VLOG(1) << msg
#define LOG_TRACE(msg) VLOG(2) << msg
#define LOG_VERBOSE(msg) VLOG(3) << msg

// Логирование запросов
inline void log_request(const std::string& method, const std::string& path, 
                       const std::string& client = "") {
    std::string msg = "Request: " + method + " " + path;
    if (!client.empty()) {
        msg += " from " + client;
    }
    LOG(INFO) << msg;
}

// Логирование ответов
inline void log_response(int status_code, const std::string& message = "") {
    std::string msg = "Response: " + std::to_string(status_code);
    if (!message.empty()) {
        msg += " - " + message;
    }
    LOG(INFO) << msg;
}

// Логирование файловых операций
inline void log_file_operation(const std::string& operation, 
                               const std::string& filename,
                               size_t size = 0) {
    std::string msg = operation + ": " + filename;
    if (size > 0) {
        msg += " (" + std::to_string(size) + " bytes)";
    }
    LOG(INFO) << msg;
}

// Логирование ошибок с контекстом
inline void log_error(const std::string& context, const std::string& message) {
    LOG(ERROR) << context << ": " << message;
}

inline void log_warning(const std::string& context, const std::string& message) {
    LOG(WARNING) << context << ": " << message;
}

} // namespace logging