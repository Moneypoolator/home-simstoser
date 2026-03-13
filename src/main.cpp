#include "server.hpp"
#include <iostream>
#include <csignal>
#include <atomic>

std::atomic<bool> g_running{true};

void signal_handler(int signal)
{
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[])
{
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
        else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  -a, --address <addr>   Bind address (default: 0.0.0.0)\n"
                      << "  -p, --port <port>      Port number (default: 9000)\n"
                      << "  -s, --storage <path>   Storage directory (default: ./storage)\n"
                      << "  -h, --help             Show this help message\n";
            return 0;
        }
    }
    
    // Установка обработчика сигналов
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        // Создаем и запускаем сервер
        s3_server server(address, port, storage_path);
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "  S3-Compatible Storage Server" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Press Ctrl+C to stop\n" << std::endl;
        
        server.run();
        
        // Ждем сигнала остановки
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        server.stop();
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}