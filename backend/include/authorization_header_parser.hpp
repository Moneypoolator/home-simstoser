#pragma once

#include <string>
#include <optional>
#include <map>

/**
 * @brief Parser for AWS Signature Version 4 Authorization headers.
 * 
 * This class provides functionality to parse Authorization headers in the
 * AWS4-HMAC-SHA256 format and extract components such as access key ID,
 * credential scope, signed headers, and signature.
 */
class authorization_header_parser {
public:
    /**
     * @brief Structure representing parsed Authorization header components.
     */
    struct parsed_auth {
        std::string access_key_id;      ///< Access key ID (e.g., "AKIA...")
        std::string credential_scope;   ///< Credential scope (e.g., "20220101/us-east-1/s3/aws4_request")
        std::string signed_headers;     ///< Signed headers (e.g., "host;x-amz-date")
        std::string signature;          ///< Signature (hex string)
        
        // For backward compatibility with existing code
        std::map<std::string, std::string> query_params; ///< Query parameters (currently unused)
    };
    
    /**
     * @brief Parse an AWS4-HMAC-SHA256 Authorization header.
     * 
     * @param auth_header The Authorization header value (e.g., "AWS4-HMAC-SHA256 Credential=...")
     * @return std::optional<parsed_auth> Parsed components if successful, std::nullopt otherwise
     */
    static std::optional<parsed_auth> parse_aws4_header(const std::string& auth_header);
    
    /**
     * @brief Extract the access key ID from an Authorization header.
     * 
     * This is a convenience method that extracts only the access key ID
     * without parsing the entire header. It's faster than full parsing
     * when only the access key ID is needed.
     * 
     * @param auth_header The Authorization header value
     * @return std::optional<std::string> Access key ID if found, std::nullopt otherwise
     */
    static std::optional<std::string> extract_access_key_id(const std::string& auth_header);
    
    /**
     * @brief Check if an Authorization header is in AWS4-HMAC-SHA256 format.
     *
     * @param auth_header The Authorization header value
     * @return true If the header starts with "AWS4-HMAC-SHA256"
     * @return false Otherwise
     */
    [[nodiscard]] static bool is_aws4_format(const std::string& auth_header);
    
    /**
     * @brief Parse a credential string (access_key_id/credential_scope).
     * 
     * @param credential_str The credential string (e.g., "AKIA.../20220101/us-east-1/s3/aws4_request")
     * @return std::pair<std::string, std::string> Pair of (access_key_id, credential_scope)
     */
    static std::pair<std::string, std::string> parse_credential(const std::string& credential_str);
    
private:
    /**
     * @brief Extract a named parameter from the Authorization header.
     * 
     * @param auth_header The Authorization header value
     * @param param_name Parameter name (e.g., "Credential=", "SignedHeaders=", "Signature=")
     * @return std::optional<std::string> Parameter value if found, std::nullopt otherwise
     */
    static std::optional<std::string> extract_param(const std::string& auth_header, const std::string& param_name);
};