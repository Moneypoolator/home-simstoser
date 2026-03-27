#pragma once


#include <string>
#include <functional>
#include <vector>
#include <optional>
#include <boost/beast.hpp>
#include "file_manager.hpp"
#include "authenticator.hpp"
#include "authorizer.hpp"

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
        authenticator* auth = nullptr,
        authorizer* authorizer = nullptr
    );
    
    template<class body, class allocator>
    void handle_request(
        http::request<body, http::basic_fields<allocator>>&& req,
        std::function<void(http::response<http::string_body>)> send
    );
    
    // Включение/выключение проверок
    void set_auth_enabled(bool enabled) { _auth_enabled = enabled; }
    void set_authorization_enabled(bool enabled) { _authorization_enabled = enabled; }
    http::response<http::string_body> handle_get(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_put(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_delete(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_list(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_get_progress(const http::request<http::string_body>& req);
    http::response<http::string_body> create_response(
        http::status status,
        const std::string& body = "",
        const std::string& content_type = "application/json"
    );
    std::string get_filename_from_path(const std::string& path) const;
private:
    friend class RequestHandlerTest;
    file_manager& _file_manager;
    authenticator* _authenticator;
    authorizer* _authorizer;
    bool _auth_enabled = false;
    bool _authorization_enabled = false;
    
    // === Аутентификация: проверка подписи запроса ===
    struct auth_result {
        bool authenticated;
        std::optional<std::string> user_id;
        std::optional<std::string> username;
    };
    
    auth_result authenticate_request(const http::request<http::string_body>& req) const;
    
    // === Авторизация: проверка прав доступа ===
    bool authorize_request(
        const std::string& user_id,
        const http::request<http::string_body>& req,
        permission_type required_permission
    ) const;
    
    // === Публичный доступ ===
    bool check_public_access(
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
    
    // Обработчики для разных методов
    
    // Multipart upload handlers
    http::response<http::string_body> handle_initiate_upload(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_upload_part(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_complete_upload(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_abort_upload(const http::request<http::string_body>& req);
    
    // Static file handler
    http::response<http::string_body> handle_static_file(const std::string& path);
    // В классе request_handler добавить:
    http::response<http::string_body> handle_index(const http::request<http::string_body>& req);
    bool is_static_file_request(const std::string& path) const;

    http::response<http::string_body> handle_openapi_spec(const http::request<http::string_body>& req);

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
};