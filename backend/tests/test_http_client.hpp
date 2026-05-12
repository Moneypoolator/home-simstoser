#pragma once

#include <string>
#include <vector>
#include <map>
#include <limits>
#include <boost/beast.hpp>
#include <boost/asio.hpp>

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

struct http_response {
    unsigned int status_code;
    std::string body;
    std::map<std::string, std::string> headers;
};

class test_http_client {
public:
    test_http_client(const std::string& host, unsigned short port)
        : _host(host), _port(port) {}
    
    http_response get(const std::string& path) {
        return send_request(http::verb::get, path, "");
    }
    
    http_response put(const std::string& path, const std::string& body) {
        return send_request(http::verb::put, path, body);
    }
    
    http_response put_file(const std::string& path, const std::vector<char>& data) {
        return send_request(http::verb::put, path, std::string(data.begin(), data.end()));
    }
    
    http_response delete_(const std::string& path) {
        return send_request(http::verb::delete_, path, "");
    }
    
private:
    std::string _host;
    unsigned short _port;
    
    http_response send_request(http::verb method, const std::string& path, const std::string& body) {
        try {
            asio::io_context io_context;
            
            tcp::resolver resolver(io_context);
            auto endpoints = resolver.resolve(_host, std::to_string(_port));
            
            beast::tcp_stream stream(io_context);
            stream.connect(endpoints);
            
            http::request<http::string_body> req{method, path, 11};
            req.set(http::field::host, _host);
            req.set(http::field::user_agent, "S3-Test-Client");
            req.set(http::field::content_type, "application/octet-stream");
            
            if (!body.empty()) {
                req.body() = body;
                req.prepare_payload();
            }
            
            http::write(stream, req);
            
            beast::flat_buffer buffer;
            http::response<http::string_body> res;
            
            // Create a parser with increased body limit to support large files
            http::response_parser<http::string_body> parser;
            parser.body_limit(std::numeric_limits<std::uint64_t>::max());
            
            http::read(stream, buffer, parser);
            res = parser.release();
            
            stream.socket().shutdown(tcp::socket::shutdown_both);
            
            http_response response;
            response.status_code = static_cast<unsigned int>(res.result_int());
            response.body = res.body();
            
            for (const auto& field : res.base()) {
                response.headers[field.name_string()] = field.value();
            }
            
            return response;
        } catch (const std::exception& e) {
            http_response error_response;
            error_response.status_code = 0;
            error_response.body = e.what();
            return error_response;
        }
    }
};