#include "server.hpp"
#include "logging.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <glog/logging.h>
#include <filesystem>

namespace fs = std::filesystem;

std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    LOG(INFO) << "Received signal " << signal << ", shutting down...";
    g_running = false;
}

int main(int argc, char* argv[])
{
    // Инициализируем glog
    google::InitGoogleLogging(argv[0]);
    FLAGS_logtostderr = true;  // Логировать в консоль
    FLAGS_colorlogtostderr = true;  // Цветное логирование
    FLAGS_log_dir = "logs";  // Директория для логов
    FLAGS_max_log_size = 100;  // Максимальный размер файла лога (МБ)
    
    // Создаем директорию для логов с помощью std::filesystem
    fs::create_directories(FLAGS_log_dir);
    
    LOG(INFO) << "========================================";
    LOG(INFO) << "  S3-Compatible Storage Server";
    LOG(INFO) << "========================================";
    LOG(INFO) << "Press Ctrl+C to stop";
    
    // Установка обработчика сигналов
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Параметры по умолчанию
    std::string address = "0.0.0.0";
    unsigned short port = 9000;
    std::string storage_path = "./storage";
    
    // Парсинг аргументов командной строки
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--address" || arg == "-a") {
            if (i + 1 < argc) address = argv[++i];
        }
        else if (arg == "--port" || arg == "-p") {
            if (i + 1 < argc) port = static_cast<unsigned short>(std::stoi(argv[++i]));
        }
        else if (arg == "--storage" || arg == "-s") {
            if (i + 1 < argc) storage_path = argv[++i];
        }
        else if (arg == "--log-level" || arg == "-l") {
            if (i + 1 < argc) {
                int vlevel = std::stoi(argv[++i]);
                FLAGS_v = vlevel;  // Уровень детализации glog
            }
        }
        else if (arg == "--log-dir") {
            if (i + 1 < argc) FLAGS_log_dir = argv[++i];
            // Создаем директорию при изменении
            fs::create_directories(FLAGS_log_dir);
        }
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  -a, --address <addr>   Bind address (default: 0.0.0.0)\n"
                      << "  -p, --port <port>      Port number (default: 9000)\n"
                      << "  -s, --storage <path>   Storage directory (default: ./storage)\n"
                      << "  -l, --log-level <lvl>  Verbosity level: 0-3 (default: 0)\n"
                      << "      --log-dir <path>   Log directory (default: ./logs)\n"
                      << "  -h, --help             Show this help message\n";
            google::ShutdownGoogleLogging();
            return 0;
        }
    }
    
    try {
        // Создаем и запускаем сервер
        s3_server server(address, port, storage_path);
        server.run();
        
        // Ждем сигнала остановки
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        server.stop();
        
        LOG(INFO) << "Server shutdown complete";
        
    } catch (const std::exception& e) {
        LOG(FATAL) << "Fatal error: " << e.what();
        google::ShutdownGoogleLogging();
        return 1;
    }
    
    google::ShutdownGoogleLogging();
    return 0;
}