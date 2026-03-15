#include "server.hpp"
#include "logging.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <filesystem>

namespace fs = std::filesystem;

std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    LOG(INFO) << "Received signal " << signal << ", shutting down...";
    g_running = false;
}

void generate_self_signed_cert(const std::string& cert_file, const std::string& key_file)
{
    LOG(INFO) << "Generating self-signed certificate...";
    
    // Проверяем, существуют ли файлы
    if (fs::exists(cert_file) && fs::exists(key_file)) {
        LOG(INFO) << "Certificate files already exist, skipping generation";
        return;
    }
    
    // Создаем директорию для сертификатов
    fs::create_directories(fs::path(cert_file).parent_path());
    
    // Генерируем самоподписанный сертификат с помощью OpenSSL
    std::string cmd = "openssl req -x509 -newkey rsa:4096 -keyout " + key_file +
                      " -out " + cert_file +
                      " -days 365 -nodes -subj \"/CN=localhost\" 2>/dev/null";
    
    int result = std::system(cmd.c_str());
    
    if (result == 0) {
        LOG(INFO) << "Self-signed certificate generated successfully";
        LOG(INFO) << "Certificate: " << cert_file;
        LOG(INFO) << "Private key: " << key_file;
    } else {
        LOG(WARNING) << "Failed to generate self-signed certificate. Please create manually.";
        LOG(INFO) << "Use: openssl req -x509 -newkey rsa:4096 -keyout " << key_file 
                  << " -out " << cert_file << " -days 365 -nodes -subj \"/CN=localhost\"";
    }
}

int main(int argc, char* argv[])
{
    // Инициализация логирования
    logging::init("s3_server", true, true, 1);
    
    LOG(INFO) << "========================================";
    LOG(INFO) << "  S3-Compatible Storage Server";
    LOG(INFO) << "========================================";
    
    // Параметры по умолчанию
    std::string address = "0.0.0.0";
    unsigned short port = 9000;
    std::string storage_path = "./storage";
    bool use_ssl = false;
    std::string cert_file = "./certs/server.crt";
    std::string key_file = "./certs/server.key";
    // Добавим параметр для файла ключей
	std::string keys_file = "";

    
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
        else if (arg == "--ssl" || arg == "-S") {
            use_ssl = true;
        }
        else if (arg == "--cert") {
            if (i + 1 < argc) cert_file = argv[++i];
        }
        else if (arg == "--key") {
            if (i + 1 < argc) key_file = argv[++i];
        }
        else if (arg == "--keys" || arg == "-k") {
            if (i + 1 < argc) keys_file = argv[++i];
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
                      << "  -S, --ssl              Enable HTTPS/SSL (default: disabled)\n"
                      << "      --cert <file>      SSL certificate file (default: ./certs/server.crt)\n"
                      << "      --key <file>       SSL private key file (default: ./certs/server.key)\n"
                      << "  -k, --keys <file>      Access keys file for authentication\n"                      
                      << "  -h, --help             Show this help message\n";
            logging::shutdown();
            return 0;
        }
    }
    
    // Установка обработчика сигналов
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        std::optional<s3_server::ssl_config> ssl_cfg;
        
        if (use_ssl) {
            // Генерируем самоподписанный сертификат, если файлы не существуют
            generate_self_signed_cert(cert_file, key_file);
            
            // Проверяем существование файлов сертификата
            if (!fs::exists(cert_file) || !fs::exists(key_file)) {
                LOG(FATAL) << "SSL certificate or key file not found!";
                LOG(FATAL) << "Please create them or use --cert and --key options";
                logging::shutdown();
                return 1;
            }
            
            ssl_cfg = s3_server::ssl_config{
                .cert_file = cert_file,
                .private_key = key_file,
                .dh_file = std::nullopt,
                .verify_client = false
            };
        }
        
        // Создаем и запускаем сервер
        s3_server server(address, port, storage_path, keys_file, ssl_cfg);
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "  S3-Compatible Storage Server" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Protocol: " << (use_ssl ? "HTTPS (SSL/TLS)" : "HTTP") << std::endl;
        std::cout << "Address: " << address << ":" << port << std::endl;
        std::cout << "Storage: " << storage_path << std::endl;
        if (use_ssl) {
            std::cout << "Certificate: " << cert_file << std::endl;
        }
        std::cout << "Press Ctrl+C to stop" << std::endl;
        std::cout << "========================================\n" << std::endl;
        
        server.run();
        
        // Ждем сигнала остановки
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        server.stop();
        
        LOG(INFO) << "Server shutdown complete";
        
    } catch (const std::exception& e) {
        LOG(FATAL) << "Fatal error: " << e.what();
        logging::shutdown();
        return 1;
    }
    
    logging::shutdown();
    return 0;
}