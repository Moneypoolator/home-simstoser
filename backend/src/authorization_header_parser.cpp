#include "authorization_header_parser.hpp"
#include <algorithm>
#include <cctype>

using namespace std;

namespace {
    // Helper function to trim whitespace
    string trim(const string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, last - first + 1);
    }

    // Helper function to strip surrounding double quotes
    string strip_quotes(const string& str) {
        if (str.length() >= 2 && str.front() == '"' && str.back() == '"') {
            return str.substr(1, str.length() - 2);
        }
        return str;
    }
}

optional<authorization_header_parser::parsed_auth> 
authorization_header_parser::parse_aws4_header(const string& auth_header) {
    if (!is_aws4_format(auth_header)) {
        return nullopt;
    }
    
    parsed_auth result;
    
    // Extract Credential
    auto credential_opt = extract_param(auth_header, "Credential=");
    if (!credential_opt) {
        return nullopt;
    }
    
    auto [access_key_id, credential_scope] = parse_credential(*credential_opt);
    if (access_key_id.empty()) {
        return nullopt;
    }
    
    result.access_key_id = access_key_id;
    result.credential_scope = credential_scope;
    
    // Extract SignedHeaders
    auto signed_headers_opt = extract_param(auth_header, "SignedHeaders=");
    if (signed_headers_opt) {
        result.signed_headers = *signed_headers_opt;
    }
    
    // Extract Signature
    auto signature_opt = extract_param(auth_header, "Signature=");
    if (!signature_opt) {
        return nullopt;
    }
    
    result.signature = *signature_opt;
    
    // Validate required fields
    if (result.access_key_id.empty() || result.signature.empty()) {
        return nullopt;
    }
    
    return result;
}

optional<string> authorization_header_parser::extract_access_key_id(const string& auth_header) {
    if (!is_aws4_format(auth_header)) {
        return nullopt;
    }
    
    auto credential_opt = extract_param(auth_header, "Credential=");
    if (!credential_opt) {
        return nullopt;
    }
    
    auto [access_key_id, _] = parse_credential(*credential_opt);
    if (access_key_id.empty()) {
        return nullopt;
    }
    
    return access_key_id;
}

bool authorization_header_parser::is_aws4_format(const string& auth_header) {
    // Check if the header starts with "AWS4-HMAC-SHA256"
    return auth_header.find("AWS4-HMAC-SHA256") == 0;
}

pair<string, string> authorization_header_parser::parse_credential(const string& credential_str) {
    size_t slash_pos = credential_str.find('/');
    if (slash_pos == string::npos) {
        return {"", ""};
    }
    
    string access_key_id = credential_str.substr(0, slash_pos);
    string credential_scope = credential_str.substr(slash_pos + 1);
    
    // Trim any whitespace
    access_key_id = trim(access_key_id);
    credential_scope = trim(credential_scope);
    
    return {access_key_id, credential_scope};
}

optional<string> authorization_header_parser::extract_param(const string& auth_header, const string& param_name) {
    size_t param_pos = auth_header.find(param_name);
    if (param_pos == string::npos) {
        return nullopt;
    }
    
    size_t start = param_pos + param_name.length();
    size_t end = auth_header.find(',', start);
    if (end == string::npos) {
        end = auth_header.length();
    }
    
    string param_value = auth_header.substr(start, end - start);
    return strip_quotes(trim(param_value));
}