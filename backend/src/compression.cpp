#include "compression.hpp"
#include <glog/logging.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

// Zlib for gzip compression
#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

// Brotli compression (optional)
#ifdef HAVE_BROTLI
#include <brotli/encode.h>
#include <brotli/decode.h>
#endif

namespace compression {

bool should_compress_content(const std::string& content_type, const compression_config& config) {
    if (!config.enabled) {
        return false;
    }
    
    // Check if content type is compressible
    static const std::vector<std::string> compressible_types = {
        "text/",
        "application/json",
        "application/javascript",
        "application/xml",
        "application/xhtml+xml",
        "image/svg+xml",
        "font/",
        "application/octet-stream" // Generic binary, but we might want to compress
    };
    
    // Check if content type matches any compressible pattern
    for (const auto& pattern : compressible_types) {
        if (content_type.find(pattern) == 0) {
            return true;
        }
    }
    
    return false;
}

std::optional<algorithm> select_algorithm(const std::string& accept_encoding, const compression_config& config) {
    if (!config.enabled || config.supported_algorithms.empty()) {
        return std::nullopt;
    }
    
    // Parse Accept-Encoding header
    std::string lower_encoding = accept_encoding;
    std::transform(lower_encoding.begin(), lower_encoding.end(), lower_encoding.begin(), ::tolower);
    
    // Check for brotli first (higher priority if client supports it)
    if (std::find(config.supported_algorithms.begin(), config.supported_algorithms.end(), algorithm::BROTLI) != config.supported_algorithms.end()) {
        if (lower_encoding.find("br") != std::string::npos && is_algorithm_supported(algorithm::BROTLI)) {
            return algorithm::BROTLI;
        }
    }
    
    // Check for gzip
    if (std::find(config.supported_algorithms.begin(), config.supported_algorithms.end(), algorithm::GZIP) != config.supported_algorithms.end()) {
        if ((lower_encoding.find("gzip") != std::string::npos || lower_encoding.find("deflate") != std::string::npos) 
            && is_algorithm_supported(algorithm::GZIP)) {
            return algorithm::GZIP;
        }
    }
    
    return std::nullopt;
}

#ifdef HAVE_ZLIB
std::vector<char> compress_gzip(const std::vector<char>& data, int level) {
    if (data.empty()) {
        return {};
    }
    
    z_stream stream;
    std::memset(&stream, 0, sizeof(stream));
    
    if (deflateInit2(&stream, level, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        LOG(ERROR) << "Failed to initialize zlib deflate";
        return {};
    }
    
    stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.data()));
    stream.avail_in = static_cast<uInt>(data.size());
    
    // Output buffer (start with input size, will grow if needed)
    std::vector<char> output(data.size());
    size_t output_size = 0;
    
    int ret;
    do {
        if (output_size + 4096 > output.capacity()) {
            output.reserve(output.capacity() * 2);
        }
        
        stream.next_out = reinterpret_cast<Bytef*>(output.data() + output_size);
        stream.avail_out = static_cast<uInt>(output.capacity() - output_size);
        
        ret = deflate(&stream, Z_FINISH);
        
        output_size = output.capacity() - stream.avail_out;
    } while (ret == Z_OK);
    
    deflateEnd(&stream);
    
    if (ret != Z_STREAM_END) {
        LOG(ERROR) << "Gzip compression failed: " << ret;
        return {};
    }
    
    output.resize(output_size);
    return output;
}
#endif

#ifdef HAVE_BROTLI
std::vector<char> compress_brotli(const std::vector<char>& data, int quality) {
    if (data.empty()) {
        return {};
    }
    
    size_t input_size = data.size();
    size_t max_output_size = BrotliEncoderMaxCompressedSize(input_size);
    
    if (max_output_size == 0) {
        LOG(ERROR) << "Brotli max compressed size calculation failed";
        return {};
    }
    
    std::vector<char> output(max_output_size);
    size_t encoded_size = max_output_size;
    
    BROTLI_BOOL result = BrotliEncoderCompress(
        quality,
        BROTLI_DEFAULT_WINDOW,
        BROTLI_MODE_GENERIC,
        input_size,
        reinterpret_cast<const uint8_t*>(data.data()),
        &encoded_size,
        reinterpret_cast<uint8_t*>(output.data())
    );
    
    if (result != BROTLI_TRUE) {
        LOG(ERROR) << "Brotli compression failed";
        return {};
    }
    
    output.resize(encoded_size);
    return output;
}
#endif

std::vector<char> compress(const std::vector<char>& data, algorithm algo, const compression_config& config) {
    // Check size limits
    if (data.size() < config.min_size || data.size() > config.max_size) {
        return {}; // Empty vector indicates no compression applied
    }
    
    switch (algo) {
#ifdef HAVE_ZLIB
        case algorithm::GZIP:
            return compress_gzip(data, config.gzip_level);
#endif
#ifdef HAVE_BROTLI
        case algorithm::BROTLI:
            return compress_brotli(data, config.brotli_quality);
#endif
        default:
            return {};
    }
}

std::vector<char> compress(const std::string& data, algorithm algo, const compression_config& config) {
    std::vector<char> vec(data.begin(), data.end());
    return compress(vec, algo, config);
}

std::string compress_string(const std::string& data, algorithm algo, const compression_config& config) {
    auto compressed = compress(data, algo, config);
    if (compressed.empty()) {
        return data; // Return original if compression failed or not applied
    }
    return std::string(compressed.begin(), compressed.end());
}

std::string content_encoding_header(algorithm algo) {
    switch (algo) {
        case algorithm::GZIP:
            return "gzip";
        case algorithm::BROTLI:
            return "br";
        default:
            return "";
    }
}

bool is_algorithm_supported(algorithm algo) {
    switch (algo) {
        case algorithm::GZIP:
#ifdef HAVE_ZLIB
            return true;
#else
            return false;
#endif
        case algorithm::BROTLI:
#ifdef HAVE_BROTLI
            return true;
#else
            return false;
#endif
        default:
            return false;
    }
}

} // namespace compression