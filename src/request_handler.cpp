#include "request_handler.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <glog/logging.h>
#include "logging.hpp"

namespace json = nlohmann;

request_handler::request_handler(file_manager& file_manager)
    : _file_manager(file_manager)
{
    VLOG(1) << "Request handler initialized";
}

template<class body, class allocator>
void request_handler::handle_request(
    http::request<body, http::basic_fields<allocator>>&& req,
    std::function<void(http::response<http::string_body>)> send)
{
    std::cout << "[" << req.method_string() << "] " << req.target() << std::endl;
    
    http::response<http::string_body> response;
    
    // CORS headers
    response.set(http::field::access_control_allow_origin, "*");
    response.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, OPTIONS");
    response.set(http::field::access_control_allow_headers, "Content-Type, Authorization");
    
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
    
    // Обслуживание статических файлов веб-интерфейса
    if (path.find("/upload/") == std::string::npos && 
        path != "/list" && 
        req.method() == http::verb::get) {
        
        // Если это не API endpoint, возвращаем статический файл
        if (path.find("/upload/") == std::string::npos) {
            response = handle_static_file(path);
            response.prepare_payload();
            send(std::move(response));
            return;
        }
    }
    
    // API endpoints
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
    else if (path == "/list" && req.method() == http::verb::get) {
        response = handle_list(req);
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
    else {
        response = create_response(
            http::status::method_not_allowed,
            R"({"error": "Method not allowed"})"
        );
    }
    
    response.prepare_payload();
    send(std::move(response));
}

// Явная инстанциация для используемых типов
template void request_handler::handle_request<http::string_body, std::allocator<char>>(
    http::request<http::string_body, http::basic_fields<std::allocator<char>>>&&,
    std::function<void(http::response<http::string_body>)>
);

http::response<http::string_body> request_handler::handle_static_file(const std::string& path)
{
    std::string filename = path;
    
    // Если запрашивается корень, возвращаем index.html
    if (filename == "/" || filename.empty()) {
        filename = "/index.html";
    }
    
    // Убираем начальный слэш
    if (!filename.empty() && filename[0] == '/') {
        filename = filename.substr(1);
    }
    
    // Определяем MIME тип по расширению
    auto get_mime_type = [](const std::string& fname) -> std::string {
        size_t dot_pos = fname.find_last_of('.');
        if (dot_pos == std::string::npos) {
            return "application/octet-stream";
        }
        
        std::string ext = fname.substr(dot_pos + 1);
        if (ext == "html" || ext == "htm") return "text/html";
        if (ext == "css") return "text/css";
        if (ext == "js") return "application/javascript";
        if (ext == "json") return "application/json";
        if (ext == "png") return "image/png";
        if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
        if (ext == "gif") return "image/gif";
        if (ext == "svg") return "image/svg+xml";
        if (ext == "ico") return "image/x-icon";
        
        return "application/octet-stream";
    };
    
    // Путь к веб-файлам (относительно исполняемого файла)
    fs::path web_dir = fs::current_path() / "web";
    fs::path file_path = web_dir / filename;
    
    // Проверяем безопасность пути
    if (!file_manager::is_path_safe(web_dir, filename)) {
        return create_response(http::status::forbidden, "Access denied");
    }
    
    // Читаем файл
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file) {
        return create_response(http::status::not_found, "File not found");
    }
    
    auto file_size = file.tellg();
    std::vector<char> file_data(file_size);
    
    file.seekg(0, std::ios::beg);
    file.read(file_data.data(), file_size);
    file.close();
    
    http::response<http::string_body> res{http::status::ok, 11};
    res.set(http::field::content_type, get_mime_type(filename));
    res.set(http::field::content_length, std::to_string(file_size));
    res.body() = std::string(file_data.begin(), file_data.end());
    
    return res;
}

http::response<http::string_body> request_handler::handle_get(const http::request<http::string_body>& req)
{
    std::string filename = get_filename_from_path(std::string(req.target()));
    
    if (filename.empty()) {
        LOG(WARNING) << "GET request with empty filename";
        return create_response(
            http::status::bad_request,
            R"({"error": "Filename is required"})"
        );
    }
    
    auto file_data = _file_manager.download_file(filename);
    
    if (!file_data) {
        LOG(WARNING) << "File not found: " << filename;
        return create_response(
            http::status::not_found,
            R"({"error": "File not found"})"
        );
    }
    
    auto metadata = _file_manager.get_metadata(filename);
    
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/octet-stream");
    res.set(http::field::content_length, std::to_string(file_data->size()));
    
    if (metadata) {
        res.set("ETag", metadata->etag);
        res.set("X-File-Size", std::to_string(metadata->size));
    }
    
    res.body() = std::string(file_data->begin(), file_data->end());
    
    LOG(INFO) << "File served successfully: " << filename;
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
        return create_response(
            http::status::bad_request,
            R"({"error": "Filename parameter is required"})"
        );
    }
    
    auto filename_opt = get_query_param(query.substr(query_pos), "filename");
    if (!filename_opt) {
        return create_response(
            http::status::bad_request,
            R"({"error": "Filename parameter is required"})"
        );
    }
    
    auto upload_id_opt = _file_manager.initiate_multipart_upload(*filename_opt);
    
    if (!upload_id_opt) {
        return create_response(
            http::status::internal_server_error,
            R"({"error": "Failed to initiate upload"})"
        );
    }
    
    json::json response_json;
    response_json["upload_id"] = *upload_id_opt;
    response_json["filename"] = *filename_opt;
    response_json["message"] = "Upload initiated successfully";
    
    return create_response(http::status::ok, response_json.dump());
}

http::response<http::string_body> request_handler::handle_upload_part(const http::request<http::string_body>& req)
{
    std::string query = std::string(req.target());
    size_t query_pos = query.find('?');
    if (query_pos == std::string::npos) {
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID and part number are required"})"
        );
    }
    
    auto upload_id_opt = get_query_param(query.substr(query_pos), "upload_id");
    auto part_number_opt = get_query_param_int(query.substr(query_pos), "part_number");
    
    if (!upload_id_opt || !part_number_opt) {
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID and part number are required"})"
        );
    }
    
    std::vector<char> data(req.body().begin(), req.body().end());
    
    if (data.empty()) {
        return create_response(
            http::status::bad_request,
            R"({"error": "Part data is required"})"
        );
    }
    
    bool success = _file_manager.upload_part(*upload_id_opt, *part_number_opt, data);
    
    if (!success) {
        return create_response(
            http::status::internal_server_error,
            R"({"error": "Failed to upload part"})"
        );
    }
    
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
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID is required"})"
        );
    }
    
    auto upload_id_opt = get_query_param(query.substr(query_pos), "upload_id");
    if (!upload_id_opt) {
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID is required"})"
        );
    }
    
    // Парсим части из тела запроса
    try {
        json::json request_json = json::json::parse(req.body());
        std::vector<int> part_numbers = request_json["parts"].get<std::vector<int>>();
        
        bool success = _file_manager.complete_multipart_upload(*upload_id_opt, part_numbers);
        
        if (!success) {
            return create_response(
                http::status::internal_server_error,
                R"({"error": "Failed to complete upload"})"
            );
        }
        
        json::json response_json;
        response_json["success"] = true;
        response_json["upload_id"] = *upload_id_opt;
        response_json["message"] = "Upload completed successfully";
        
        return create_response(http::status::ok, response_json.dump());
    } catch (const json::json::exception& e) {
    	LOG(ERROR) << "Exception handling request: " << e.what();
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
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID is required"})"
        );
    }
    
    auto upload_id_opt = get_query_param(query.substr(query_pos), "upload_id");
    if (!upload_id_opt) {
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID is required"})"
        );
    }
    
    bool success = _file_manager.abort_multipart_upload(*upload_id_opt);
    
    if (!success) {
        return create_response(
            http::status::not_found,
            R"({"error": "Upload not found"})"
        );
    }
    
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
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID is required"})"
        );
    }
    
    auto upload_id_opt = get_query_param(query.substr(query_pos), "upload_id");
    if (!upload_id_opt) {
        return create_response(
            http::status::bad_request,
            R"({"error": "Upload ID is required"})"
        );
    }
    
    auto progress_opt = _file_manager.get_upload_progress(*upload_id_opt);
    
    if (!progress_opt) {
        return create_response(
            http::status::not_found,
            R"({"error": "Upload not found"})"
        );
    }
    
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
    
    // Если путь - это "list", возвращаем пустую строку
    if (clean_path == "list") {
        return "";
    }
    
    return clean_path;
}


std::optional<std::string> request_handler::get_query_param(const std::string& query, const std::string& param) const
{
    size_t pos = query.find(param + "=");
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    
    pos += param.length() + 1;
    size_t end_pos = query.find('&', pos);
    
    if (end_pos == std::string::npos) {
        return query.substr(pos);
    }
    
    return query.substr(pos, end_pos - pos);
}

std::optional<int> request_handler::get_query_param_int(const std::string& query, const std::string& param) const
{
    auto str_opt = get_query_param(query, param);
    if (!str_opt) {
        return std::nullopt;
    }
    
    try {
        return std::stoi(*str_opt);
    } catch (...) {
        return std::nullopt;
    }
}