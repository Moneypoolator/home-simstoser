#pragma once


#include <string>
#include <functional>
#include <optional>
#include <memory>
#include <boost/beast.hpp>
#include <glog/logging.h>
#include "file_manager.hpp"
#include "authenticator.hpp"
#include "authorizer.hpp"
#include "server.hpp"  // for cors_config
#include "logging.hpp"
#include "compression.hpp"
#include "metrics.hpp"

namespace beast = boost::beast;
namespace http = beast::http;

// Типы разрешений для авторизации
// enum class permission_type {
//     READ,
//     WRITE,
//     DELETE,
//     LIST,
//     MANAGE_ACL
// };

class RequestHandlerTest; // forward declaration for friend

class request_handler {
public:
    explicit request_handler(
        file_manager& file_manager,
        std::shared_ptr<authenticator> auth = nullptr,
        std::shared_ptr<authorizer> authorizer = nullptr
    );

    // template <class Body, class Allocator, class Send>
    // void handle_request(
    //     http::request<Body, http::basic_fields<Allocator>>&& req,
    //     Send&& send);

    template <class Body, class Allocator>
    void handle_request(
        http::request<Body, http::basic_fields<Allocator>>&& req,
        std::function<void(http::response<http::string_body>)> send)
    {
        VLOG(1) << "Handling request: " << req.method_string() << " " << req.target();
        
        // Start timing for metrics
        metrics::ScopedTimer timer(std::string(req.method_string()), std::string(req.target()));
        
        http::response<http::string_body> response;
        apply_cors_headers(response, req);
        
        // CORS preflight
        if (req.method() == http::verb::options) {
            VLOG(2) << "Handling CORS preflight request";
            response.result(http::status::ok);
            response.set(http::field::content_type, "text/plain");
            response.body() = "OK";
            apply_compression_if_needed(response, req);
            response.prepare_payload();
            // Record metrics for preflight request
            metrics::MetricsCollector::instance().record_request(
                std::string(req.method_string()),
                std::string(req.target()),
                static_cast<int>(response.result()),
                timer.elapsed()
            );
            send(std::move(response));
            return;
        }
        
        std::string path = std::string(req.target());
        std::string normalized_path = path;
        if (!normalized_path.empty() && normalized_path[0] == '/') {
            normalized_path = normalized_path.substr(1);
        }
        if (normalized_path.compare(0, 4, "api/") == 0) {
            normalized_path = normalized_path.substr(4);
        }
        
        // Обслуживание статических файлов веб-интерфейса
        if (is_static_file_request(path)) {
            response = handle_static_file(path);
            apply_cors_headers(response, req);
            apply_compression_if_needed(response, req);
            response.prepare_payload();
            // Record metrics for static file request
            metrics::MetricsCollector::instance().record_request(
                std::string(req.method_string()),
                std::string(req.target()),
                static_cast<int>(response.result()),
                timer.elapsed()
            );
            send(std::move(response));
            return;
        }
        
        // Определяем требуемое разрешение для запроса
        auto required_perm = get_required_permission(req);
        VLOG(2) << "Required permission: " << (required_perm ? permission_to_string(*required_perm) : "none");
        
        // Аутентификация
        LOG(INFO) << "handle_request: calling authenticate_request for path=" << path;
        auth_result auth_result = authenticate_request(req);
        LOG(INFO) << "Authentication result: authenticated=" << auth_result.authenticated << ", user_id=" << (auth_result.user_id ? *auth_result.user_id : "none");
        
        // Авторизация или публичный доступ
        bool access_granted = false;
        std::string user_context = "anonymous";
        
        if (auth_result.authenticated && auth_result.user_id) {
            if (!required_perm || authorize_request(*auth_result.user_id, req, *required_perm)) {
                access_granted = true;
                user_context = auth_result.username.value_or(*auth_result.user_id);
                VLOG(2) << "Access granted to authenticated user: " << user_context;
            } else {
                response = create_error_response(
                    http::status::forbidden,
                    "AccessDenied",
                    "Access denied"
                );
            }
        } else if (required_perm) {
            if (!_auth_enabled && !_authorization_enabled) {
                access_granted = true;
                VLOG(2) << "Access granted (auth disabled)";
            } else if (check_public_access(req, *required_perm)) {
                access_granted = true;
                VLOG(2) << "Access granted via public access";
            } else {
                response = create_error_response(
                    http::status::unauthorized,
                    "InvalidSignature",
                    "Signature validation failed"
                );
            }
        } else {
            access_granted = true;
        }
        
        if (!access_granted) {
            apply_compression_if_needed(response, req);
            response.prepare_payload();
            // Record metrics for denied request
            metrics::MetricsCollector::instance().record_request(
                std::string(req.method_string()),
                std::string(req.target()),
                static_cast<int>(response.result()),
                timer.elapsed()
            );
            send(std::move(response));
            return;
        }
        
        // Обработка запроса по пути и методу
        try {
            // Metrics endpoint (should be before other GET handlers)
            if (path == "/metrics" && req.method() == http::verb::get) {
                response = handle_metrics(req);
            }
            else if (path.find("/upload/initiate") != std::string::npos && req.method() == http::verb::post) {
                response = handle_initiate_upload(req);
            }
            else if (path.find("/upload/part") != std::string::npos && req.method() == http::verb::put) {
                response = handle_upload_part(req);
            }
            else if (path.find("/upload/complete") != std::string::npos && req.method() == http::verb::post) {
                response = handle_complete_upload(req);
            }
            else if (path.find("/upload/abort") != std::string::npos && req.method() == http::verb::delete_) {
                response = handle_abort_upload(req);
            }
            else if (path.find("/upload/progress") != std::string::npos && req.method() == http::verb::get) {
                response = handle_get_progress(req);
            }
            else if (normalized_path == "list" && req.method() == http::verb::get) {
                response = handle_list(req);
            }
            else if (req.method() == http::verb::get && (path == "/files" || path == "/users" || path == "/keys" || path == "/policies" || path == "/settings" || path == "/dashboard" || path == "/login")) {
                response = handle_static_file("/");
            }
            else if (req.method() == http::verb::get) {
                response = handle_get(req);
            }
            else if (req.method() == http::verb::put) {
                response = handle_put(req);
            }
            else if (req.method() == http::verb::delete_) {
                response = handle_delete(req);
            }
            else if ((path == "/openapi.yaml" || path == "/api/spec") && req.method() == http::verb::get) {
                response = handle_openapi_spec(req);
            }
            else {
                response = create_response(
                    http::status::method_not_allowed,
                    R"({"error": "Method not allowed"})"
                );
            }
        } catch (const std::exception& e) {
            LOG(ERROR) << "Exception handling request: " << e.what();
            response = create_error_response(
                http::status::internal_server_error,
                "InternalError",
                "Internal server error"
            );
        }
        
        VLOG(3) << "Applying CORS headers";
        apply_cors_headers(response, req);
        VLOG(3) << "Applying compression if needed";
        apply_compression_if_needed(response, req);
        VLOG(3) << "Preparing payload";
        response.prepare_payload();
        // Record metrics for the request
        VLOG(3) << "Recording metrics";
        metrics::MetricsCollector::instance().record_request(
            std::string(req.method_string()),
            std::string(req.target()),
            static_cast<int>(response.result()),
            timer.elapsed()
        );
        VLOG(3) << "Sending response";
        send(std::move(response));
    }

    // Включение/выключение проверок
    void set_auth_enabled(bool enabled) {
        LOG(INFO) << "request_handler::set_auth_enabled(" << enabled << ")";
        _auth_enabled = enabled;
    }
    void set_authorization_enabled(bool enabled) {
        LOG(INFO) << "request_handler::set_authorization_enabled(" << enabled << ")";
        _authorization_enabled = enabled;
    }
    
    // Настройка CORS
    void set_cors_config(const std::optional<s3_server::cors_config>& config) { _cors_config = config; }
    
    // Настройка сжатия
    void set_compression_config(const compression::compression_config& config) { _compression_config = config; }
    
    http::response<http::string_body> handle_get(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_put(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_delete(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_list(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_get_progress(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_metrics(const http::request<http::string_body>& req);

    http::response<http::string_body> create_response(
        http::status status,
        const std::string& body = "",
        const std::string& content_type = "application/json"
    );
    std::string get_filename_from_path(const std::string& path) const;

    http::response<http::file_body> handle_get_file_body(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_upload_part(const http::request<http::string_body>& req);

    // Multipart upload handlers
    http::response<http::string_body> handle_initiate_upload(const http::request<http::string_body>& req);
    // http::response<http::string_body> handle_upload_part(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_complete_upload(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_abort_upload(const http::request<http::string_body>& req);
    
    // Static file handler
    http::response<http::string_body> handle_static_file(const std::string& path);
    // В классе request_handler добавить:
    http::response<http::string_body> handle_index(const http::request<http::string_body>& req);

    // Применение CORS заголовков к ответу
    template<class Body>
    void apply_cors_headers(http::response<Body>& response, const http::request<http::string_body>& req) const;


    // === Аутентификация: проверка подписи запроса ===
    struct auth_result {
        bool authenticated;
        std::optional<std::string> user_id;
        std::optional<std::string> username;
    };

    auth_result authenticate_request(const http::request<http::string_body>& req) const;

    http::response<http::string_body> handle_openapi_spec(const http::request<http::string_body>& req);

// private:
//     friend class RequestHandlerTest;
//     friend class RequestHandlerAuthTest;

    file_manager& _file_manager;
    std::shared_ptr<authenticator> _authenticator;
    std::shared_ptr<authorizer> _authorizer;
    bool _auth_enabled = false;
    bool _authorization_enabled = false;
    std::optional<s3_server::cors_config> _cors_config;
    compression::compression_config _compression_config;
    

    

    
    // === Авторизация: проверка прав доступа ===
    [[nodiscard]] bool authorize_request(
        const std::string& user_id,
        const http::request<http::string_body>& req,
        permission_type required_permission
    ) const;
    
    // === Публичный доступ ===
    [[nodiscard]] bool check_public_access(
        const http::request<http::string_body>& req,
        permission_type required_permission
    ) const;
    
    // Извлечение пути к ресурсу из запроса
    std::string extract_resource_path(const http::request<http::string_body>& req) const;
    
    // Определение требуемого разрешения для запроса
    std::optional<permission_type> get_required_permission(
        const http::request<http::string_body>& req
    ) const;
    
    // Преобразование permission_type в строку
    static std::string permission_to_string(permission_type perm);
    
    // URL декодирование
    static std::string url_decode(const std::string& encoded);
    
    
    // Обработчики для разных методов
    // http::response<http::file_body> handle_get_file(const std::string& filename);
    
    

    [[nodiscard]] bool is_static_file_request(const std::string& path) const;


    // Вспомогательные функции
    
    http::response<http::string_body> create_error_response(
        http::status status,
        const std::string& code,
        const std::string& message
    );
    
    std::optional<std::string> get_query_param(const std::string& query, const std::string& param) const;
    std::optional<int> get_query_param_int(const std::string& query, const std::string& param) const;
    
    // Преобразование заголовков в map
    std::map<std::string, std::string> get_headers_map(const http::request<http::string_body>& req) const;

    // Парсинг заголовка Range (bytes=start-end)
    // Возвращает пару (start, end) или std::nullopt если заголовок отсутствует или некорректен
    std::optional<std::pair<size_t, size_t>> parse_range_header(const http::request<http::string_body>& req, size_t file_size) const;

    // Применение сжатия к ответу (если поддерживается клиентом и конфигурацией)
    template<class Body>
    void apply_compression_if_needed(
        http::response<Body>& response,
        const http::request<http::string_body>& req
    ) const;
    
    // Установка заголовка Accept-Ranges в ответ
    template<class Body>
    void set_accept_ranges_header(http::response<Body>& response) const;
};