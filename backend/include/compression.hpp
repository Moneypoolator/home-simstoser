#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>

namespace compression {

enum class algorithm {
    NONE,
    GZIP,
    BROTLI
};

struct compression_config {
    bool enabled = true;
    size_t min_size = 1024; // Minimum size in bytes to compress
    size_t max_size = 10 * 1024 * 1024; // Maximum size in bytes to compress (10MB)
    std::vector<algorithm> supported_algorithms = {algorithm::GZIP};
    
    // Quality/speed tradeoff (1-9 for gzip, 0-11 for brotli)
    int gzip_level = 6;
    int brotli_quality = 4;
    
    // Whether to compress static files (HTML, CSS, JS)
    bool compress_static = true;
    
    // Whether to compress API responses (JSON)
    bool compress_api = true;
    
    // Whether to compress file downloads
    bool compress_files = true;
};

// Check if compression should be applied based on content type
bool should_compress_content(const std::string& content_type, const compression_config& config);

// Get the best compression algorithm based on Accept-Encoding header
std::optional<algorithm> select_algorithm(const std::string& accept_encoding, const compression_config& config);

// Compress data using the specified algorithm
std::vector<char> compress(const std::vector<char>& data, algorithm algo, const compression_config& config);
std::vector<char> compress(const std::string& data, algorithm algo, const compression_config& config);

// Compress data and return as string
std::string compress_string(const std::string& data, algorithm algo, const compression_config& config);

// Get content encoding header value for algorithm
std::string content_encoding_header(algorithm algo);

// Check if algorithm is supported (library available)
bool is_algorithm_supported(algorithm algo);

} // namespace compression