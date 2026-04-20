#include "request_handler.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <glog/logging.h>
#include "logging.hpp"

namespace json = nlohmann;

// MIME типы для статических файлов
static std::map<std::string, std::string> mime_types = {
    {".html", "text/html"},
    {".htm", "text/html"},
    {".js", "application/javascript"},
    {".mjs", "application/javascript"},
    {".json", "application/json"},
    {".css", "text/css"},
    {".png", "image/png"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif", "image/gif"},
    {".svg", "image/svg+xml"},
    {".ico", "image/x-icon"},
    {".woff", "font/woff"},
    {".woff2", "font/woff2"},
    {".ttf", "font/ttf"},
    {".eot", "application/vnd.ms-fontobject"},
    {".otf", "font/otf"}
};

request_handler::request_handler(
    file_manager& file_manager,
    std::shared_ptr<authenticator> auth,
    std::shared_ptr<authorizer> authorizer)
    : _file_manager(file_manager)
    , _authenticator(auth)
    , _authorizer(authorizer)
{
    VLOG(1) << "Request handler initialized (auth: " 
            << (_authenticator ? "enabled" : "disabled") 
            << ", authorization: " 
            << (_authorizer ? "enabled" : "disabled") << ")";
}

// ========== АУТЕНТИФИКАЦИЯ ==========

request_handler::auth_result request_handler::authenticate_request(
    const http::request<http::string_body>& req) const
{
    if (!_auth_enabled || !_authenticator) {
        VLOG(2) << "Authentication disabled, skipping";
        return {false, std::nullopt, std::nullopt};
    }
    
    // Собираем заголовки
    std::map<std::string, std::string> headers;
    for (const auto& field : req.base()) {
        std::string name = std::string(field.name_string());
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        headers[name] = std::string(field.value());
    }
    
    // Проверяем подпись запроса
    bool valid = _authenticator->verify_signature(
        std::string(req.method_string()),
        std::string(req.target()),
        headers,
        req.body()
    );
    
    if (!valid) {
        LOG(WARNING) << "Authentication failed: " 
                     << req.method_string() << " " << req.target();
        return {false, std::nullopt, std::nullopt};
    }
    
    // Извлекаем user_id из заголовка Authorization
    auto auth_it = headers.find("authorization");
    if (auth_it != headers.end()) {
        // Парсим заголовок для извлечения access_key_id
        std::string auth_header = auth_it->second;
        
        // Извлекаем access_key_id из заголовка Authorization
        // Формат: AWS4-HMAC-SHA256 Credential=AKIA..., ...
        size_t cred_pos = auth_header.find("Credential=");
        if (cred_pos != std::string::npos) {
            size_t start = cred_pos + 11;
            size_t end = auth_header.find(',', start);
            if (end == std::string::npos) end = auth_header.length();
            
            std::string cred_str = auth_header.substr(start, end - start);
            size_t slash_pos = cred_str.find('/');
            
            if (slash_pos != std::string::npos) {
                std::string access_key_id = cred_str.substr(0, slash_pos);
                
                // Получаем ключ доступа
                auto access_key_opt = _authenticator->get_key(access_key_id);
                if (access_key_opt) {
                    VLOG(2) << "Authenticated with access key: " << access_key_id;
                    return {true, access_key_id, access_key_opt->user_name};
                }
            }
        }
    }
    
    LOG(WARNING) << "Authentication successful but access key not found";
    return {true, std::nullopt, std::nullopt};
}

// ========== АВТОРИЗАЦИЯ ==========

bool request_handler::authorize_request(
    const std::string& user_id,
    const http::request<http::string_body>& req,
    permission_type required_permission) const
{
    if (!_authorization_enabled || !_authorizer) {
        VLOG(2) << "Authorization disabled, allowing request";
        return true;
    }
    
    // Извлекаем путь к ресурсу
    std::string resource_path = extract_resource_path(req);
    
    // Проверяем доступ
    bool authorized = _authorizer->check_access(
        user_id,
        resource_path,
        required_permission
    );
    
    if (!authorized) {
        LOG(WARNING) << "Authorization failed for user " << user_id 
                     << " to resource " << resource_path 
                     << " (permission: " 
                     << permission_to_string(required_permission) << ")";
        return false;
    }
    
    VLOG(2) << "Authorization successful for user " << user_id 
            << " to resource " << resource_path;
    return true;
}

// ========== ПУБЛИЧНЫЙ ДОСТУП ==========

bool request_handler::check_public_access(
    const http::request<http::string_body>& req,
    permission_type required_permission) const
{
    if (!_authorizer) {
        VLOG(2) << "Authorizer not available, public access disabled";
        return false;
    }
    
    // Извлекаем путь к ресурсу
    std::string resource_path = extract_resource_path(req);
    
    // Проверяем публичный доступ
    bool public_access = _authorizer->check_public_access(
        resource_path,
        required_permission
    );
    
    if (public_access) {
        VLOG(2) << "Public access granted to resource: " << resource_path;
    }
    
    return public_access;
}

// Добавим метод для преобразования заголовков
std::map<std::string, std::string> request_handler::get_headers_map(
    const http::request<http::string_body>& req) const
{
    std::map<std::string, std::string> headers;
    
    for (const auto& field : req.base()) {
        // Преобразуем имя заголовка в нижний регистр
        std::string name = std::string(field.name_string());
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        
        headers[name] = std::string(field.value());
    }
    
    return headers;
}

std::string request_handler::extract_resource_path(
    const http::request<http::string_body>& req) const
{
    std::string path = std::string(req.target());
    
    // Удаляем query string
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        path = path.substr(0, query_pos);
    }
    
    // Удаляем начальный слэш
    if (!path.empty() && path[0] == '/') {
        path = path.substr(1);
    }

    // Если путь начинается с "api/", удаляем этот префикс
    if (path.compare(0, 4, "api/") == 0) {
        path = path.substr(4);
    }

    // Для специальных эндпоинтов возвращаем пустой путь
    if (path.find("upload/") != std::string::npos || path == "list") {
        return "";
    }

    return path;
}

std::optional<permission_type> request_handler::get_required_permission(
    const http::request<http::string_body>& req) const
{
    std::string path = std::string(req.target());
    
    // Специальные эндпоинты не требуют разрешений
    if (path.find("/upload/") != std::string::npos) {
        return std::nullopt;
    }
    
    // Нормализуем путь: удаляем начальный слэш и префикс /api/
    std::string normalized = path;
    if (!normalized.empty() && normalized[0] == '/') {
        normalized = normalized.substr(1);
    }
    if (normalized.compare(0, 4, "api/") == 0) {
        normalized = normalized.substr(4);
    }
    
    // Определяем разрешение по методу
    if (req.method() == http::verb::get) {
        if (normalized == "list") {
            return permission_type::LIST;
        }
        return permission_type::READ;
    }
    else if (req.method() == http::verb::put) {
        return permission_type::WRITE;
    }
    else if (req.method() == http::verb::delete_) {
        return permission_type::DELETE;
    }
    
    return std::nullopt;
}

std::string request_handler::permission_to_string(permission_type perm) {
    switch (perm) {
        case permission_type::READ: return "READ";
        case permission_type::WRITE: return "WRITE";
        case permission_type::DELETE: return "DELETE";
        case permission_type::LIST: return "LIST";
        case permission_type::MANAGE_ACL: return "MANAGE_ACL";
        default: return "UNKNOWN";
    }
}

// ========== ОСНОВНАЯ ОБРАБОТКА ЗАПРОСОВ ==========
/*
template <class Body, class Allocator, class Send>
void request_handler::handle_request(
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send)
{
    VLOG(1) << "Handling request: " << req.method_string() << " " << req.target();
    
    http::response<http::string_body> response;
    
    // Apply CORS headers (configurable)
    apply_cors_headers(response, req);

    // CORS preflight
    if (req.method() == http::verb::options) {
        VLOG(2) << "Handling CORS preflight request";
        response.result(http::status::ok);
        response.set(http::field::content_type, "text/plain");
        response.body() = "OK";
        response.prepare_payload();
        send(std::move(response));
        return;
    }
    
    std::string path = std::string(req.target());
    
    // Нормализуем путь: удаляем начальный слэш и префикс /api/
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
        // Apply CORS headers to static file response
        apply_cors_headers(response, req);
        response.prepare_payload();
        send(std::move(response));
        return;
    }

    // if (path.find("/upload/") == std::string::npos &&
    //     path != "/list" &&
    //     req.method() == http::verb::get) {

    //     if (path.find("/upload/") == std::string::npos) {
    //         response = handle_static_file(path);
    //         response.prepare_payload();
    //         send(std::move(response));
    //         return;
    //     }
    // }
    
    // Определяем требуемое разрешение для запроса
    auto required_perm = get_required_permission(req);
    
    // === ЭТАП 1: АУТЕНТИФИКАЦИЯ ===
    auth_result auth_result = authenticate_request(req);
    
    // === ЭТАП 2: АВТОРИЗАЦИЯ или ПУБЛИЧНЫЙ ДОСТУП ===
    bool access_granted = false;
    std::string user_context = "anonymous";
    
    if (auth_result.authenticated && auth_result.user_id) {
        // Аутентификация успешна, проверяем авторизацию
        if (!required_perm || authorize_request(*auth_result.user_id, req, *required_perm)) {
            access_granted = true;
            user_context = auth_result.username.value_or(*auth_result.user_id);
            VLOG(2) << "Access granted to authenticated user: " << user_context;
        } else {
            // Авторизация не пройдена
            response = create_error_response(
                http::status::forbidden,
                "AccessDenied",
                "Access denied"
            );
        }
    } else if (required_perm) {
        // Если аутентификация и авторизация отключены, разрешаем доступ
        if (!_auth_enabled && !_authorization_enabled) {
            access_granted = true;
            VLOG(2) << "Access granted (auth disabled)";
        }
        // Аутентификация неуспешна, проверяем публичный доступ
        else if (check_public_access(req, *required_perm)) {
            access_granted = true;
            VLOG(2) << "Access granted via public access";
        } else {
            // Публичный доступ запрещен
            response = create_error_response(
                http::status::unauthorized,
                "InvalidSignature",
                "Signature validation failed"
            );
        }
    } else {
        // Запрос не требует аутентификации (например, multipart upload)
        access_granted = true;
    }
    
    // Если доступ не разрешен, отправляем ответ с ошибкой
    if (!access_granted) {
        response.prepare_payload();
        send(std::move(response));
        return;
    }
    
    // === ЭТАП 3: ОБРАБОТКА ЗАПРОСА ===
    try {
        if (path.find("/upload/initiate") != std::string::npos && req.method() == http::verb::post) {
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
    
    // Apply CORS headers to all responses
    apply_cors_headers(response, req);
    
    response.prepare_payload();
    send(std::move(response));
}
*/
// // Явная инстанциация для используемых типов
// template void request_handler::handle_request<http::string_body, std::allocator<char>>(
//     http::request<http::string_body, http::basic_fields<std::allocator<char>>>&&,
//     std::function<void(http::response<http::string_body>)>
// );

// template<class Body, class Allocator, class Send>
// void request_handler::handle_request(
//     http::request<Body, http::basic_fields<Allocator>>&& req,
//     Send&& send);

// http::response<http::file_body> request_handler::handle_get_file(const std::string& filename) {
//     // Проверки безопасности, существования файла...
//     fs::path file_path = _file_manager.get_full_path(filename); // новый метод
    
//     http::response<http::file_body> res{http::status::ok, 11};
//     res.body().open(file_path.c_str(), boost::beast::file_mode::read);
//     res.set(http::field::content_type, "application/octet-stream");
//     res.set(http::field::content_length, std::to_string(fs::file_size(file_path)));
//     return res;
// }

http::response<http::string_body> request_handler::handle_get(
    const http::request<http::string_body>& req)
{
    std::string filename = get_filename_from_path(std::string(req.target()));
    if (filename.empty()) {
        // Check if this is a /list API request
        std::string target = std::string(req.target());
        // Normalize path similar to get_filename_from_path
        std::string clean_path = target;
        size_t query_pos = clean_path.find('?');
        if (query_pos != std::string::npos) {
            clean_path = clean_path.substr(0, query_pos);
        }
        if (!clean_path.empty() && clean_path[0] == '/') {
            clean_path = clean_path.substr(1);
        }
        if (clean_path.compare(0, 4, "api/") == 0) {
            clean_path = clean_path.substr(4);
        }
        if (clean_path == "list") {
            return handle_list(req);
        }
        return create_response(http::status::bad_request, R"({"error": "Filename required"})");
    }
    
    fs::path full_path = _file_manager.get_full_path(filename);
    if (!fs::exists(full_path) || !fs::is_regular_file(full_path)) {
        return create_response(http::status::not_found, R"({"error": "File not found"})");
    }
    
    auto file_size = fs::file_size(full_path);
    std::ifstream file(full_path, std::ios::binary);
    if (!file) {
        return create_response(http::status::internal_server_error, R"({"error": "Cannot open file"})");
    }
    
    // Резервируем память
    std::string body;
    body.reserve(static_cast<size_t>(file_size));
    
    // Буфер 1 MB
    constexpr size_t buffer_size = 1024 * 1024;
    std::vector<char> buffer(buffer_size);
    
    while (file) {
        file.read(buffer.data(), buffer_size);
        std::streamsize bytes = file.gcount();
        if (bytes > 0) {
            body.append(buffer.data(), static_cast<size_t>(bytes));
        }
    }
    
    file.close();
    
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/octet-stream");
    res.set(http::field::content_length, std::to_string(file_size));
    res.set("X-File-Size", std::to_string(file_size));
    
    auto metadata = _file_manager.get_metadata(filename);
    if (metadata) {
        res.set("ETag", metadata->etag);
        // Add custom metadata headers
        for (const auto& [key, value] : metadata->custom_metadata) {
            std::string header_name = "x-amz-meta-" + key;
            res.set(header_name, value);
        }
    }
    
    res.body() = std::move(body);
    return res;
}

http::response<http::string_body> request_handler::handle_put(const http::request<http::string_body>& req)
{
    std::string filename = get_filename_from_path(std::string(req.target()));
    
    if (filename.empty()) {
        LOG(WARNING) << "PUT request with empty filename";
        return create_response(
            http::status::bad_request,
            R"({"error": "Filename is required"})"
        );
    }
    
    std::vector<char> data(req.body().begin(), req.body().end());
    
    if (data.empty()) {
        LOG(WARNING) << "PUT request with empty body for file: " << filename;
        return create_response(
            http::status::bad_request,
            R"({"error": "File content is required"})"
        );
    }
    
    bool success = _file_manager.upload_file(filename, data);
    
    if (!success) {
        LOG(ERROR) << "Failed to upload file: " << filename;
        return create_response(
            http::status::internal_server_error,
            R"({"error": "Failed to upload file"})"
        );
    }
    
    // Extract custom metadata headers (x-amz-meta-*)
    std::map<std::string, std::string> custom_metadata;
    auto headers = get_headers_map(req);
    for (const auto& [header_name, value] : headers) {
        if (header_name.compare(0, 11, "x-amz-meta-") == 0) {
            std::string meta_key = header_name.substr(11); // remove prefix
            custom_metadata[meta_key] = value;
        }
    }
    if (!custom_metadata.empty()) {
        _file_manager.set_custom_metadata(filename, custom_metadata);
    }
    
    auto metadata = _file_manager.get_metadata(filename);
    
    json::json response_json;
    response_json["success"] = true;
    response_json["filename"] = filename;
    response_json["size"] = data.size();
    if (metadata) {
        response_json["etag"] = metadata->etag;
    }
    
    LOG(INFO) << "File uploaded successfully: " << filename << " ("
              << std::to_string(data.size()) << " bytes)";
    return create_response(http::status::created, response_json.dump());
}

http::response<http::string_body> request_handler::handle_delete(const http::request<http::string_body>& req)
{
    std::string filename = get_filename_from_path(std::string(req.target()));
    
    if (filename.empty()) {
        LOG(WARNING) << "DELETE request with empty filename";
        return create_response(
            http::status::bad_request,
            R"({"error": "Filename is required"})"
        );
    }
    
    if (!_file_manager.file_exists(filename)) {
        LOG(WARNING) << "Attempted to delete non-existent file: " << filename;
        return create_response(
            http::status::not_found,
            R"({"error": "File not found"})"
        );
    }
    
    bool success = _file_manager.delete_file(filename);
    
    if (!success) {
        LOG(ERROR) << "Failed to delete file: " << filename;
        return create_response(
            http::status::internal_server_error,
            R"({"error": "Failed to delete file"})"
        );
    }
    
    json::json response_json;
    response_json["success"] = true;
    response_json["message"] = "File deleted successfully";
    
    LOG(INFO) << "File deleted successfully: " << filename;
    return create_response(http::status::ok, response_json.dump());
}

http::response<http::string_body> request_handler::handle_list(const http::request<http::string_body>& /*req*/)
{
    auto files = _file_manager.list_files();
    
    json::json response_json;
    response_json["count"] = files.size();
    response_json["files"] = json::json::array();
    
    for (const auto& file : files) {
        json::json file_json;
        file_json["name"] = file.name;
        file_json["size"] = file.size;
        file_json["etag"] = file.etag;
        
        // Форматируем время в ISO 8601
        auto time_point = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            file.last_modified - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
        std::time_t time = std::chrono::system_clock::to_time_t(time_point);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");
        file_json["last_modified"] = ss.str();
        
        response_json["files"].push_back(file_json);
    }
    
    LOG(INFO) << "Listed " << std::to_string(files.size()) << " files";
    return create_response(http::status::ok, response_json.dump());
}


http::response<http::string_body> request_handler::handle_initiate_upload(const http::request<http::string_body>& req)
{
    // Получаем имя файла из query параметра
    std::string query = std::string(req.target());
    size_t query_pos = query.find('?');
    if (query_pos == std::string::npos) {
        LOG(WARNING) << "Initiate upload request without filename parameter";
        return create_response(
            http::status::bad_request,
            R"({"error": "Filename parameter is required"})"
        );
    }
    
    auto filename_opt = get_query_param(query.substr(query_pos), "filename");
    if (!filename_opt) {
        LOG(WARNING) << "Initiate upload request missing filename parameter";
        return create_response(
            http::status::bad_request,
            R"({"error": "Filename parameter is required"})"
        );
    }
    
    LOG(INFO) << "Initiating multipart upload for: " << *filename_opt;
    
    auto upload_id_opt = _file_manager.initiate_multipart_upload(*filename_opt);
    
    if (!upload_id_opt) {
        LOG(ERROR) << "Failed to initiate multipart upload for: " << *filename_opt;
        return create_response(
            http::status::internal_server_error,
            R"({"error": "Failed to initiate upload"})"
        );
    }
    
    LOG(INFO) << "Multipart upload initiated: " << *filename_opt 
              << " (ID: " << *upload_id_opt << ")";
    
    json::json response_json;
    response_json["upload_id"] = *upload_id_opt;
    response_json["filename"] = *filename_opt;
    response_json["message"] = "Upload initiated successfully";
    
    return create_response(http::status::ok, response_json.dump());
}

bool request_handler::is_static_file_request(const std::string& path) const
{
    // Статические файлы обслуживаются из /assets/ и / (index.html)
    return path.find("/assets/") == 0 || 
           path == "/" || 
           path.find(".html") != std::string::npos ||
           path.find(".js") != std::string::npos ||
           path.find(".css") != std::string::npos ||
           path.find(".png") != std::string::npos ||
           path.find(".jpg") != std::string::npos ||
           path.find(".svg") != std::string::npos ||
           path.find(".ico") != std::string::npos ||
           path.find(".woff") != std::string::npos ||
           path.find(".ttf") != std::string::npos;
}

http::response<http::string_body> request_handler::handle_static_file(const std::string& path)
{
    try {

        std::string filename = path;

        // Если запрашивается корень, возвращаем index.html
        if (filename == "/" || filename.empty()) {
            filename = "index.html";
        }

        // Убираем начальный слэш
        if (!filename.empty() && filename[0] == '/') {
            filename = filename.substr(1);
        }

        VLOG(1) << "Serving static file: " << filename;

        // Путь к веб-файлам (относительно исполняемого файла)
        // Используем путь относительно исполняемого файла: ../web/dist
        fs::path exe_dir = fs::current_path();
        fs::path project_root = exe_dir.parent_path(); // Поднимаемся на уровень выше из build/
        fs::path web_dir = project_root / "web" / "dist";
        fs::path file_path = web_dir / filename;

        // Проверяем безопасность пути
        // if (!file_manager::is_path_safe(web_dir, filename)) {
        fs::path resolved_path = fs::absolute(file_path);
        fs::path base_path = fs::absolute(web_dir);

        if (resolved_path.string().substr(0, base_path.string().length()) != base_path.string()) {
            LOG(WARNING) << "Unsafe static file path access attempt: " << filename;
            return create_response(http::status::forbidden, "Access denied");
        }

        // Читаем файл
        std::ifstream file(file_path, std::ios::binary | std::ios::ate);
        if (!file) {
            // Если файл не найден, возвращаем index.html для SPA
            if (path != "/") {
                VLOG(1) << "File not found, serving index.html: " << filename;
                return handle_static_file("/");
            }
            VLOG(1) << "Static file not found: " << filename;
            return create_response(http::status::not_found, "File not found");
        }

        auto file_size = file.tellg();
        std::vector<char> file_data(file_size);

        file.seekg(0, std::ios::beg);
        file.read(file_data.data(), file_size);
        file.close();

        VLOG(1) << "Static file served: " << filename << " (" << file_size << " bytes)";

        // Определяем MIME тип
        std::string extension = file_path.extension().string();

        std::string content_type = "application/octet-stream";
        auto it = mime_types.find(extension);
        if (it != mime_types.end()) {
            content_type = it->second;
        }

        http::response<http::string_body> res { http::status::ok, 11 };
        res.set(http::field::content_type, content_type);
        res.set(http::field::content_length, std::to_string(file_size));
        res.set(http::field::cache_control, "max-age=3600");
        // Добавляем CORS заголовки для поддержки crossorigin атрибутов
        res.set(http::field::access_control_allow_origin, "*");
        res.set(http::field::access_control_allow_methods, "GET, HEAD, OPTIONS");
        res.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
        res.body() = std::string(file_data.begin(), file_data.end());

        return res;

    } catch (const std::exception& e) {
        LOG(ERROR) << "Error serving static file " << path << ": " << e.what();
        return create_response(http::status::internal_server_error, "Internal server error");
    }
}

http::response<http::string_body> request_handler::handle_index(const http::request<http::string_body>& /*req*/)
{
    return handle_static_file("/");
}

http::response<http::string_body> request_handler::handle_openapi_spec(const http::request<http::string_body>& req)
{
    // Читаем файл openapi.yaml
    std::ifstream file("openapi.yaml");
    if (!file) {
        return create_response(http::status::not_found, "Specification not found");
    }
    
    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/yaml");
    res.body() = content;
    return res;
}

http::response<http::string_body> request_handler::handle_upload_part(const http::request<http::string_body>& req)
{
    std::string query = std::string(req.target());
    size_t query_pos = query.find('?');
    if (query_pos == std::string::npos) {
        LOG(WARNING) << "Upload part request without required parameters";
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID and part number are required"})"
        );
    }
    
    auto upload_id_opt = get_query_param(query.substr(query_pos), "upload_id");
    auto part_number_opt = get_query_param_int(query.substr(query_pos), "part_number");
    
    if (!upload_id_opt || !part_number_opt) {
        LOG(WARNING) << "Upload part request missing required parameters";
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID and part number are required"})"
        );
    }
    
    std::vector<char> data(req.body().begin(), req.body().end());
    
    if (data.empty()) {
        LOG(WARNING) << "Upload part request with empty body: upload_id=" << *upload_id_opt 
                     << ", part=" << *part_number_opt;
        return create_response(
            http::status::bad_request,
            R"({"error": "Part data is required"})"
        );
    }
    
    LOG(INFO) << "Uploading part: upload_id=" << *upload_id_opt 
              << ", part=" << *part_number_opt << ", size=" << data.size() << " bytes";
    
    bool success = _file_manager.upload_part(*upload_id_opt, *part_number_opt, data);
    
    if (!success) {
        LOG(ERROR) << "Failed to upload part: upload_id=" << *upload_id_opt 
                   << ", part=" << *part_number_opt;
        return create_response(
            http::status::internal_server_error,
            R"({"error": "Failed to upload part"})"
        );
    }
    
    LOG(INFO) << "Part uploaded successfully: upload_id=" << *upload_id_opt 
              << ", part=" << *part_number_opt;
    
    json::json response_json;
    response_json["success"] = true;
    response_json["upload_id"] = *upload_id_opt;
    response_json["part_number"] = *part_number_opt;
    response_json["size"] = data.size();
    
    return create_response(http::status::ok, response_json.dump());
}

http::response<http::string_body> request_handler::handle_complete_upload(const http::request<http::string_body>& req)
{
    std::string query = std::string(req.target());
    size_t query_pos = query.find('?');
    if (query_pos == std::string::npos) {
        LOG(WARNING) << "Complete upload request without upload_id parameter";
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID is required"})"
        );
    }
    
    auto upload_id_opt = get_query_param(query.substr(query_pos), "upload_id");
    if (!upload_id_opt) {
        LOG(WARNING) << "Complete upload request missing upload_id parameter";
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID is required"})"
        );
    }
    
    // Парсим части из тела запроса
    try {
        json::json request_json = json::json::parse(req.body());
        std::vector<int> part_numbers = request_json["parts"].get<std::vector<int>>();
        
        if (part_numbers.empty()) {
            LOG(WARNING) << "Complete upload with empty parts list: " << *upload_id_opt;
            return create_response(
                http::status::bad_request,
                R"({"error": "Parts list cannot be empty"})"
            );
        }
        
        LOG(INFO) << "Completing upload: upload_id=" << *upload_id_opt 
                  << ", parts=" << part_numbers.size();
        
        bool success = _file_manager.complete_multipart_upload(*upload_id_opt, part_numbers);
        
        if (!success) {
            LOG(ERROR) << "Failed to complete upload: " << *upload_id_opt;
            return create_response(
                http::status::internal_server_error,
                R"({"error": "Failed to complete upload"})"
            );
        }
        
        LOG(INFO) << "Upload completed successfully: " << *upload_id_opt;
        
        json::json response_json;
        response_json["success"] = true;
        response_json["upload_id"] = *upload_id_opt;
        response_json["message"] = "Upload completed successfully";
        
        return create_response(http::status::ok, response_json.dump());
    } catch (const json::json::exception& e) {
        LOG(ERROR) << "Exception handling complete upload request: " << e.what();
        return create_response(
            http::status::bad_request,
            R"({"error": "Invalid request body"})"
        );
    }
}

http::response<http::string_body> request_handler::handle_abort_upload(const http::request<http::string_body>& req)
{
    std::string query = std::string(req.target());
    size_t query_pos = query.find('?');
    if (query_pos == std::string::npos) {
        LOG(WARNING) << "Abort upload request without upload_id parameter";
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID is required"})"
        );
    }
    
    auto upload_id_opt = get_query_param(query.substr(query_pos), "upload_id");
    if (!upload_id_opt) {
        LOG(WARNING) << "Abort upload request missing upload_id parameter";
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID is required"})"
        );
    }
    
    LOG(INFO) << "Aborting upload: " << *upload_id_opt;
    
    bool success = _file_manager.abort_multipart_upload(*upload_id_opt);
    
    if (!success) {
        LOG(WARNING) << "Attempted to abort non-existent upload: " << *upload_id_opt;
        return create_response(
            http::status::not_found,
            R"({"error": "Upload not found"})"
        );
    }
    
    LOG(INFO) << "Upload aborted successfully: " << *upload_id_opt;
    
    json::json response_json;
    response_json["success"] = true;
    response_json["upload_id"] = *upload_id_opt;
    response_json["message"] = "Upload aborted successfully";
    
    return create_response(http::status::ok, response_json.dump());
}

http::response<http::string_body> request_handler::handle_get_progress(const http::request<http::string_body>& req)
{
    std::string query = std::string(req.target());
    size_t query_pos = query.find('?');
    if (query_pos == std::string::npos) {
        LOG(WARNING) << "Get progress request without upload_id parameter";
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID is required"})"
        );
    }
    
    auto upload_id_opt = get_query_param(query.substr(query_pos), "upload_id");
    if (!upload_id_opt) {
        LOG(WARNING) << "Get progress request missing upload_id parameter";
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID is required"})"
        );
    }
    
    auto progress_opt = _file_manager.get_upload_progress(*upload_id_opt);
    
    if (!progress_opt) {
        VLOG(1) << "Progress requested for non-existent upload: " << *upload_id_opt;
        return create_response(
            http::status::not_found,
            R"({"error": "Upload not found"})"
        );
    }
    
    VLOG(1) << "Progress retrieved for upload: " << *upload_id_opt 
            << ", parts=" << progress_opt->size();
    
    json::json response_json;
    response_json["upload_id"] = *upload_id_opt;
    response_json["parts"] = json::json::object();
    
    for (const auto& [part_num, size] : *progress_opt) {
        response_json["parts"][std::to_string(part_num)] = size;
    }
    
    return create_response(http::status::ok, response_json.dump());
}

http::response<http::string_body> request_handler::create_response(
    http::status status,
    const std::string& body,
    const std::string& content_type)
{
    http::response<http::string_body> res{status, 11};
    res.set(http::field::content_type, content_type);
    res.set(http::field::access_control_allow_origin, "*");
    res.body() = body;
    res.prepare_payload();
    
    VLOG(2) << "Creating response: status=" << static_cast<int>(status)
            << ", content_type=" << content_type
            << ", body_size=" << body.size();
    
    return res;
}

// Добавим метод для создания ошибки в формате S3
http::response<http::string_body> request_handler::create_error_response(
    http::status status,
    const std::string& code,
    const std::string& message)
{
    // Формат ошибки S3
    std::string error_xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<Error>
    <Code>)" + code + R"(</Code>
    <Message>)" + message + R"(</Message>
    <RequestId>00000000-0000-0000-0000-000000000000</RequestId>
</Error>)";
    
    http::response<http::string_body> res{status, 11};
    res.set(http::field::content_type, "application/xml");
    res.set(http::field::access_control_allow_origin, "*");
    res.body() = error_xml;
    
    return res;
}

std::string request_handler::get_filename_from_path(const std::string& path) const
{
    // Убираем начальный слэш и параметры запроса
    std::string clean_path = path;
    
    // Удаляем query string
    size_t query_pos = clean_path.find('?');
    if (query_pos != std::string::npos) {
        clean_path = clean_path.substr(0, query_pos);
    }
    
    // Удаляем начальный слэш
    if (!clean_path.empty() && clean_path[0] == '/') {
        clean_path = clean_path.substr(1);
    }

    // Если путь начинается с "api/", удаляем этот префикс
    if (clean_path.compare(0, 4, "api/") == 0) {
        clean_path = clean_path.substr(4);
    }

    // Если путь - это "list", возвращаем пустую строку
    if (clean_path == "list") {
        return "";
    }

    VLOG(2) << "Extracted filename from path: " << path << " -> " << clean_path;

    // Декодируем URL-encoded символы (например, русские буквы)
    std::string decoded = url_decode(clean_path);
    VLOG(2) << "Decoded filename: " << decoded;
    return decoded;
}

http::response<http::file_body> request_handler::handle_get_file_body(const http::request<http::string_body>& req)
{
    std::string filename = get_filename_from_path(std::string(req.target()));
    if (filename.empty()) {
        throw std::invalid_argument("Empty filename");
    }
    
    // Получаем полный путь к файлу
    fs::path full_path = _file_manager.get_full_path(filename);
    
    // Проверяем существование файла
    if (!fs::exists(full_path) || !fs::is_regular_file(full_path)) {
        throw std::runtime_error("File not found: " + filename);
    }
    
    auto file_size = fs::file_size(full_path);
    
    // Создаём ответ с file_body
    http::response<http::file_body> res{http::status::ok, req.version()};
    
    // Открываем файл для чтения
    boost::beast::error_code ec;
    res.body().open(full_path.c_str(), boost::beast::file_mode::read, ec);
    if (ec) {
        LOG(ERROR) << "Failed to open file for streaming: " << full_path
                   << ", error: " << ec.message();
        throw std::runtime_error("Cannot open file");
    }
    
    // Устанавливаем заголовки
    res.set(http::field::content_type, "application/octet-stream");
    res.set(http::field::content_length, std::to_string(file_size));
    res.set("X-File-Size", std::to_string(file_size));
    
    // Добавляем ETag, если есть
    auto metadata = _file_manager.get_metadata(filename);
    if (metadata) {
        res.set("ETag", metadata->etag);
    }
    
    LOG(INFO) << "Streaming file (file_body): " << filename << " (" << file_size << " bytes)";
    return res;
}

std::string request_handler::url_decode(const std::string& encoded) {
    std::string result;
    result.reserve(encoded.size());
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            int hex1 = encoded[i + 1];
            int hex2 = encoded[i + 2];
            if (std::isxdigit(hex1) && std::isxdigit(hex2)) {
                int value = (hex1 <= '9' ? hex1 - '0' : (hex1 <= 'F' ? hex1 - 'A' + 10 : hex1 - 'a' + 10)) * 16 +
                            (hex2 <= '9' ? hex2 - '0' : (hex2 <= 'F' ? hex2 - 'A' + 10 : hex2 - 'a' + 10));
                result += static_cast<char>(value);
                i += 2;
            } else {
                result += encoded[i];
            }
        } else if (encoded[i] == '+') {
            result += ' ';
        } else {
            result += encoded[i];
        }
    }
    return result;
}


// ========== CORS HEADERS ==========

template<class Body>
void request_handler::apply_cors_headers(http::response<Body>& response, const http::request<http::string_body>& req) const
{
    // If CORS is not configured, use default permissive headers
    if (!_cors_config.has_value()) {
        response.set(http::field::access_control_allow_origin, "*");
        response.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, OPTIONS");
        response.set(http::field::access_control_allow_headers, "Content-Type, Authorization, X-Amz-Date, X-Amz-Security-Token, X-Access-Key");
        return;
    }
    
    const auto& config = _cors_config.value();
    
    // Get the Origin header from the request
    std::string origin;
    if (req.find(http::field::origin) != req.end()) {
        origin = std::string(req[http::field::origin]);
    }
    
    // Set Access-Control-Allow-Origin
    std::string allow_origin = config.get_allowed_origins_header(origin);
    response.set(http::field::access_control_allow_origin, allow_origin);
    
    // Add Vary: Origin header when Access-Control-Allow-Origin is not a wildcard
    // This informs caches that the response varies based on the Origin header
    if (allow_origin != "*") {
        response.set(http::field::vary, "Origin");
    }
    
    // Set Access-Control-Allow-Methods
    response.set(http::field::access_control_allow_methods, config.get_allowed_methods_header());
    
    // Set Access-Control-Allow-Headers
    response.set(http::field::access_control_allow_headers, config.get_allowed_headers_header());
    
    // Set Access-Control-Expose-Headers if configured
    std::string exposed_headers = config.get_exposed_headers_header();
    if (!exposed_headers.empty()) {
        response.set(http::field::access_control_expose_headers, exposed_headers);
    }
    
    // Set Access-Control-Allow-Credentials if enabled
    if (config.allow_credentials) {
        response.set(http::field::access_control_allow_credentials, "true");
    }
    
    // Set Access-Control-Max-Age for preflight requests
    if (req.method() == http::verb::options && config.max_age > 0) {
        response.set(http::field::access_control_max_age, std::to_string(config.max_age));
    }
    
    VLOG(3) << "Applied CORS headers for request from origin: " << (origin.empty() ? "(none)" : origin);
}

// Explicit template instantiation for the response types we use
template void request_handler::apply_cors_headers<http::string_body>(http::response<http::string_body>&, const http::request<http::string_body>&) const;
template void request_handler::apply_cors_headers<http::empty_body>(http::response<http::empty_body>&, const http::request<http::string_body>&) const;

std::optional<std::string> request_handler::get_query_param(const std::string& query, const std::string& param) const
{
    size_t pos = query.find(param + "=");
    if (pos == std::string::npos) {
        VLOG(2) << "Query parameter not found: " << param;
        return std::nullopt;
    }
    
    pos += param.length() + 1;
    size_t end_pos = query.find('&', pos);
    
    if (end_pos == std::string::npos) {
        VLOG(2) << "Query parameter found: " << param;
        return query.substr(pos);
    }
    
    VLOG(2) << "Query parameter found: " << param;
    return query.substr(pos, end_pos - pos);
}

std::optional<int> request_handler::get_query_param_int(const std::string& query, const std::string& param) const
{
    auto str_opt = get_query_param(query, param);
    if (!str_opt) {
        VLOG(2) << "Query parameter not found (int): " << param;
        return std::nullopt;
    }
    
    try {
        int value = std::stoi(*str_opt);
        VLOG(2) << "Query parameter parsed (int): " << param << " = " << value;
        return value;
    } catch (...) {
        LOG(WARNING) << "Failed to parse query parameter as int: " << param << " = " << *str_opt;
        return std::nullopt;
    }
}