#pragma once


#include <string>
#include <functional>
#include <vector>
#include <boost/beast.hpp>
#include "file_manager.hpp"
#include "authenticator.hpp"

namespace beast = boost::beast;
namespace http = beast::http;

class request_handler {
public:
    explicit request_handler(file_manager& file_manager, authenticator* auth = nullptr);
    
    template<class body, class allocator>
    void handle_request(
        http::request<body, http::basic_fields<allocator>>&& req,
        std::function<void(http::response<http::string_body>)> send
    );
    
    // Включение/выключение аутентификации
    void set_authentication_enabled(bool enabled) { _auth_enabled = enabled; }
    
private:
    file_manager& _file_manager;
    authenticator* _authenticator;
    bool _auth_enabled = false;
    
    // Проверка аутентификации
    bool authenticate_request(const http::request<http::string_body>& req) const;
    
    // Обработчики для разных методов
    http::response<http::string_body> handle_get(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_put(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_delete(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_list(const http::request<http::string_body>& req);
    
    // Multipart upload handlers
    http::response<http::string_body> handle_initiate_upload(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_upload_part(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_complete_upload(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_abort_upload(const http::request<http::string_body>& req);
    http::response<http::string_body> handle_get_progress(const http::request<http::string_body>& req);
    
    // Static file handler
    http::response<http::string_body> handle_static_file(const std::string& path);
    
    // Вспомогательные функции
    http::response<http::string_body> create_response(
        http::status status,
        const std::string& body = "",
        const std::string& content_type = "application/json"
    );
    
    http::response<http::string_body> create_error_response(
        http::status status,
        const std::string& code,
        const std::string& message
    );
    
    std::string get_filename_from_path(const std::string& path) const;
    std::optional<std::string> get_query_param(const std::string& query, const std::string& param) const;
    std::optional<int> get_query_param_int(const std::string& query, const std::string& param) const;
    
    // Преобразование заголовков в map
    std::map<std::string, std::string> get_headers_map(const http::request<http::string_body>& req) const;
};