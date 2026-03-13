#pragma once


#include <string>
#include <functional>
#include <boost/beast.hpp>
#include "file_manager.hpp"

namespace beast = boost::beast;
namespace http = beast::http;

class request_handler {
public:
    explicit request_handler(file_manager& file_manager);
    
    template<class body, class allocator>
    void handle_request(
        http::request<body, http::basic_fields<allocator>>&& req,
        std::function<void(http::response<http::string_body>)> send
    );

private:
    file_manager& _file_manager;
    
    http::response<http::string_body> handle_static_file(const std::string& path);

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

    // Вспомогательные функции
    http::response<http::string_body> create_response(
        http::status status,
        const std::string& body = "",
        const std::string& content_type = "application/json"
    );
    
    std::string get_filename_from_path(const std::string& path) const;
    std::optional<std::string> get_query_param(const std::string& query, const std::string& param) const;
    std::optional<int> get_query_param_int(const std::string& query, const std::string& param) const;
};