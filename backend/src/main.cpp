#include "server.hpp"
#include "logging.hpp"
#include <iostream>
#include <csignal>
#include <atomic>
#include <filesystem>
#include <ctime>
#include <cstdio>
#include <algorithm>

namespace fs = std::filesystem;

std::atomic<bool> g_running{true};

void signal_handler(int signal)
{
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
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

// Check if certificate is expired or about to expire (within 30 days)
bool is_certificate_expiring_soon(const std::string& cert_file, int days_threshold = 30)
{
    if (!fs::exists(cert_file)) {
        return true; // Certificate doesn't exist, needs generation
    }
    
    // Use OpenSSL to check certificate expiration
    std::string cmd = "openssl x509 -enddate -noout -in " + cert_file + " 2>/dev/null";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        LOG(WARNING) << "Failed to check certificate expiration";
        return false;
    }
    
    char buffer[256];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    
    // Parse the date from output like "notAfter=Dec 31 23:59:59 2025 GMT"
    size_t pos = result.find("notAfter=");
    if (pos == std::string::npos) {
        LOG(WARNING) << "Failed to parse certificate expiration date";
        return false;
    }
    
    std::string date_str = result.substr(pos + 9); // Skip "notAfter="
    // Remove trailing newline
    date_str.erase(std::remove(date_str.begin(), date_str.end(), '\n'), date_str.end());
    
    // Convert date string to time_t
    std::tm tm = {};
    strptime(date_str.c_str(), "%b %d %H:%M:%S %Y GMT", &tm);
    std::time_t expiration_time = std::mktime(&tm);
    
    // Get current time
    std::time_t current_time = std::time(nullptr);
    
    // Calculate days until expiration
    double seconds_until_expiration = std::difftime(expiration_time, current_time);
    int days_until_expiration = static_cast<int>(seconds_until_expiration / (60 * 60 * 24));
    
    LOG(INFO) << "Certificate expires in " << days_until_expiration << " days";
    
    return days_until_expiration <= days_threshold;
}

// Renew certificate if it's expiring soon
void renew_certificate_if_needed(const std::string& cert_file, const std::string& key_file, int days_threshold = 30)
{
    if (is_certificate_expiring_soon(cert_file, days_threshold)) {
        LOG(INFO) << "Certificate is expiring soon or doesn't exist, renewing...";
        
        // Backup old certificate files
        std::string backup_cert = cert_file + ".backup";
        std::string backup_key = key_file + ".backup";
        
        if (fs::exists(cert_file)) {
            fs::copy_file(cert_file, backup_cert, fs::copy_options::overwrite_existing);
        }
        if (fs::exists(key_file)) {
            fs::copy_file(key_file, backup_key, fs::copy_options::overwrite_existing);
        }
        
        // Generate new certificate
        generate_self_signed_cert(cert_file, key_file);
        
        LOG(INFO) << "Certificate renewed successfully";
    } else {
        LOG(INFO) << "Certificate is still valid, no renewal needed";
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
    std::string keys_file = "./access_keys.csv";
    std::string users_file = "./users.csv";
    bool enable_auth = true;
    bool enable_ssl = false;
    
    bool use_ssl = false;
    bool use_letsencrypt = false;
    std::string cert_file = "./certs/server.crt";
    std::string key_file = "./certs/server.key";
    std::string letsencrypt_dir = "";
    
    // CORS configuration
    bool enable_cors = true;
    std::vector<std::string> cors_origins = {"*"};
    std::vector<std::string> cors_methods = {"GET", "POST", "PUT", "DELETE", "OPTIONS", "HEAD"};
    std::vector<std::string> cors_headers = {"Content-Type", "Authorization", "X-Amz-Date", "X-Amz-Security-Token", "X-Requested-With", "X-Access-Key"};
    std::vector<std::string> cors_exposed_headers = {"ETag", "X-File-Size", "X-Upload-Id"};
    bool cors_allow_credentials = false;
    int cors_max_age = 86400; // 24 hours

    upload_limits_config default_limits;
    default_limits.max_file_size = 5ULL * 1024 * 1024 * 1024; // 5 GB
    default_limits.max_part_size = 100 * 1024 * 1024; // 100 MB
    default_limits.max_parts_per_upload = 10000; // как в S3
    default_limits.max_temp_storage_total = 20ULL * 1024 * 1024 * 1024; // 20 GB

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
        else if (arg == "--keys" || arg == "-k") {
            if (i + 1 < argc) keys_file = argv[++i];
        }
        else if (arg == "--users" || arg == "-u") {
            if (i + 1 < argc) users_file = argv[++i];
        } else if (arg == "--max-file-size") {
            if (i + 1 < argc)
                default_limits.max_file_size = std::stoull(argv[++i]);
        } else if (arg == "--max-part-size") {
            if (i + 1 < argc)
                default_limits.max_part_size = std::stoull(argv[++i]);
        } else if (arg == "--max-parts") {
            if (i + 1 < argc)
                default_limits.max_parts_per_upload = std::stoull(argv[++i]);
        } else if (arg == "--max-temp-storage") {
            if (i + 1 < argc)
                default_limits.max_temp_storage_total = std::stoull(argv[++i]);
        } else if (arg == "--no-auth") {
            enable_auth = false;
        } else if (arg == "--ssl" || arg == "-S") {
            use_ssl = true;
            enable_ssl = true;
        } else if (arg == "--cert") {
            if (i + 1 < argc) cert_file = argv[++i];
        } else if (arg == "--key") {
            if (i + 1 < argc) key_file = argv[++i];
        } else if (arg == "--letsencrypt" || arg == "-L") {
            use_letsencrypt = true;
            use_ssl = true;
            enable_ssl = true;
            if (i + 1 < argc) letsencrypt_dir = argv[++i];
        } else if (arg == "--log-level" || arg == "-l") {
            if (i + 1 < argc) {
                int vlevel = std::stoi(argv[++i]);
                FLAGS_v = vlevel;  // Уровень детализации glog
            }
        } else if (arg == "--log-dir") {
            if (i + 1 < argc) FLAGS_log_dir = argv[++i];
            // Создаем директорию при изменении
            fs::create_directories(FLAGS_log_dir);
        } else if (arg == "--no-cors") {
            enable_cors = false;
        } else if (arg == "--cors-origins") {
            if (i + 1 < argc) {
                std::string origins = argv[++i];
                cors_origins.clear();
                size_t pos = 0;
                while ((pos = origins.find(',')) != std::string::npos) {
                    cors_origins.push_back(origins.substr(0, pos));
                    origins.erase(0, pos + 1);
                }
                if (!origins.empty()) {
                    cors_origins.push_back(origins);
                }
            }
        } else if (arg == "--cors-methods") {
            if (i + 1 < argc) {
                std::string methods = argv[++i];
                cors_methods.clear();
                size_t pos = 0;
                while ((pos = methods.find(',')) != std::string::npos) {
                    cors_methods.push_back(methods.substr(0, pos));
                    methods.erase(0, pos + 1);
                }
                if (!methods.empty()) {
                    cors_methods.push_back(methods);
                }
            }
        } else if (arg == "--cors-headers") {
            if (i + 1 < argc) {
                std::string headers = argv[++i];
                cors_headers.clear();
                size_t pos = 0;
                while ((pos = headers.find(',')) != std::string::npos) {
                    cors_headers.push_back(headers.substr(0, pos));
                    headers.erase(0, pos + 1);
                }
                if (!headers.empty()) {
                    cors_headers.push_back(headers);
                }
            }
        } else if (arg == "--cors-exposed-headers") {
            if (i + 1 < argc) {
                std::string exposed = argv[++i];
                cors_exposed_headers.clear();
                size_t pos = 0;
                while ((pos = exposed.find(',')) != std::string::npos) {
                    cors_exposed_headers.push_back(exposed.substr(0, pos));
                    exposed.erase(0, pos + 1);
                }
                if (!exposed.empty()) {
                    cors_exposed_headers.push_back(exposed);
                }
            }
        } else if (arg == "--cors-credentials") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                cors_allow_credentials = (val == "true" || val == "1" || val == "yes");
            }
        } else if (arg == "--cors-max-age") {
            if (i + 1 < argc) {
                cors_max_age = std::stoi(argv[++i]);
            }
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                      << "Options:\n"
                      << "  -a, --address <addr>   Bind address (default: 0.0.0.0)\n"
                      << "  -p, --port <port>      Port number (default: 9000)\n"
                      << "  -s, --storage <path>   Storage directory (default: ./storage)\n"
                      << "  -k, --keys <file>      Keys file (default: ./keys.json)\n"
                      << "  -u, --users <file>     Users file (default: ./users.json)\n"
                      << "      --no-auth          Disable authentication\n"
                      << "      --ssl              Enable SSL/TLS\n"
                      << "      --letsencrypt, -L <dir> Use Let's Encrypt certificates from directory\n"
                      << "      --cert <file>      SSL certificate file\n"
                      << "      --key <file>       SSL private key file\n"
                      << "      --no-cors          Disable CORS (default: enabled with permissive settings)\n"
                      << "      --cors-origins <list>     Comma-separated allowed origins (default: *)\n"
                      << "      --cors-methods <list>     Comma-separated allowed methods (default: GET,POST,PUT,DELETE,OPTIONS,HEAD)\n"
                      << "      --cors-headers <list>     Comma-separated allowed headers\n"
                      << "      --cors-exposed-headers <list> Comma-separated exposed headers\n"
                      << "      --cors-credentials <bool>  Allow credentials (true/false, default: false)\n"
                      << "      --cors-max-age <seconds>   Preflight cache duration (default: 86400)\n"
                      << "      --max-file-size <bytes>\n"
                      << "      --max-part-size <bytes>\n"
                      << "      --max-parts <number>\n"
                      << "      --max-temp-storage <bytes>\n"
                      << "  -h, --help             Show this help message\n";
            logging::shutdown();
            return 0;
        }
    }
    
    // Если аутентификация отключена, очищаем файлы ключей и пользователей
    if (!enable_auth) {
        keys_file = "";
        users_file = "";
    }

    // Установка обработчика сигналов
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    try {
        std::optional<s3_server::ssl_config> ssl_cfg;
        
        if (use_ssl) {
            if (use_letsencrypt) {
                // Use Let's Encrypt certificate paths
                if (letsencrypt_dir.empty()) {
                    // Default Let's Encrypt directory
                    letsencrypt_dir = "/etc/letsencrypt/live/" + address;
                }
                
                std::string le_cert = letsencrypt_dir + "/fullchain.pem";
                std::string le_key = letsencrypt_dir + "/privkey.pem";
                
                // Check if Let's Encrypt certificates exist
                if (!fs::exists(le_cert) || !fs::exists(le_key)) {
                    LOG(FATAL) << "Let's Encrypt certificates not found in: " << letsencrypt_dir;
                    LOG(FATAL) << "Expected files: " << le_cert << " and " << le_key;
                    LOG(FATAL) << "Please run certbot to obtain certificates first";
                    logging::shutdown();
                    return 1;
                }
                
                cert_file = le_cert;
                key_file = le_key;
                
                LOG(INFO) << "Using Let's Encrypt certificates from: " << letsencrypt_dir;
                
                // Check Let's Encrypt certificate expiration (warn if < 7 days)
                if (is_certificate_expiring_soon(cert_file, 7)) {
                    LOG(WARNING) << "Let's Encrypt certificate is expiring soon!";
                    LOG(WARNING) << "Run 'certbot renew' to renew certificates";
                }
            } else {
                // Check and renew self-signed certificate if needed (expiring within 30 days)
                renew_certificate_if_needed(cert_file, key_file, 30);
            }
            
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
        
        // Create CORS configuration if enabled
        std::optional<s3_server::cors_config> cors_cfg;
        if (enable_cors) {
            cors_cfg = s3_server::cors_config{
                .allowed_origins = cors_origins,
                .allowed_methods = cors_methods,
                .allowed_headers = cors_headers,
                .exposed_headers = cors_exposed_headers,
                .allow_credentials = cors_allow_credentials,
                .max_age = cors_max_age
            };
        }
        
        s3_server server(address, port, storage_path, keys_file, users_file, ssl_cfg, cors_cfg);
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "  S3-Compatible Storage Server" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Protocol: " << (use_ssl ? "HTTPS (SSL/TLS)" : "HTTP") << std::endl;
        std::cout << "Address: " << address << ":" << port << std::endl;
        std::cout << "Storage: " << storage_path << std::endl;
        if (!keys_file.empty()) {
            std::cout << "Authentication: ENABLED (keys: " << keys_file << ")" << std::endl;
        }
        if (!users_file.empty()) {
            std::cout << "Authorization: ENABLED (users: " << users_file << ")" << std::endl;
        }
        if (use_ssl) {
            if (use_letsencrypt) {
                std::cout << "Certificate: Let's Encrypt (" << cert_file << ")" << std::endl;
            } else {
                std::cout << "Certificate: Self-signed (" << cert_file << ")" << std::endl;
            }
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