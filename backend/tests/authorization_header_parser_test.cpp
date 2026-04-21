#include <gtest/gtest.h>
#include "authorization_header_parser.hpp"

TEST(AuthorizationHeaderParserTest, IsAws4Format) {
    EXPECT_TRUE(authorization_header_parser::is_aws4_format(
        "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20220101/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=abcdef"
    ));
    
    EXPECT_TRUE(authorization_header_parser::is_aws4_format(
        "AWS4-HMAC-SHA256 Credential=AKIA..."
    ));
    
    EXPECT_FALSE(authorization_header_parser::is_aws4_format(
        "Basic dXNlcjpwYXNz"
    ));
    
    EXPECT_FALSE(authorization_header_parser::is_aws4_format(
        "Bearer token"
    ));
    
    EXPECT_FALSE(authorization_header_parser::is_aws4_format(""));
}

TEST(AuthorizationHeaderParserTest, ParseCredential) {
    auto [access_key_id, credential_scope] = authorization_header_parser::parse_credential(
        "AKIAIOSFODNN7EXAMPLE/20220101/us-east-1/s3/aws4_request"
    );
    
    EXPECT_EQ(access_key_id, "AKIAIOSFODNN7EXAMPLE");
    EXPECT_EQ(credential_scope, "20220101/us-east-1/s3/aws4_request");
    
    // With spaces
    auto [ak2, cs2] = authorization_header_parser::parse_credential(
        "  AKIAEXAMPLE  /  20220101/us-east-1/s3/aws4_request  "
    );
    
    EXPECT_EQ(ak2, "AKIAEXAMPLE");
    EXPECT_EQ(cs2, "20220101/us-east-1/s3/aws4_request");
    
    // Invalid credential (no slash)
    auto [ak3, cs3] = authorization_header_parser::parse_credential("AKIAEXAMPLE");
    EXPECT_TRUE(ak3.empty());
    EXPECT_TRUE(cs3.empty());
    
    // Empty string
    auto [ak4, cs4] = authorization_header_parser::parse_credential("");
    EXPECT_TRUE(ak4.empty());
    EXPECT_TRUE(cs4.empty());
}

TEST(AuthorizationHeaderParserTest, ExtractAccessKeyId) {
    // Valid header
    auto result = authorization_header_parser::extract_access_key_id(
        "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20220101/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=abcdef"
    );
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "AKIAIOSFODNN7EXAMPLE");
    
    // With spaces
    result = authorization_header_parser::extract_access_key_id(
        "AWS4-HMAC-SHA256 Credential= AKIAEXAMPLE/20220101/us-east-1/s3/aws4_request , SignedHeaders=host;x-amz-date, Signature=abcdef"
    );
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "AKIAEXAMPLE");
    
    // Missing Credential parameter
    result = authorization_header_parser::extract_access_key_id(
        "AWS4-HMAC-SHA256 SignedHeaders=host;x-amz-date, Signature=abcdef"
    );
    
    EXPECT_FALSE(result.has_value());
    
    // Not AWS4 format
    result = authorization_header_parser::extract_access_key_id(
        "Basic dXNlcjpwYXNz"
    );
    
    EXPECT_FALSE(result.has_value());
    
    // Empty header
    result = authorization_header_parser::extract_access_key_id("");
    EXPECT_FALSE(result.has_value());
}

TEST(AuthorizationHeaderParserTest, ParseAws4Header) {
    // Valid full header
    std::string header = 
        "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20220101/us-east-1/s3/aws4_request, "
        "SignedHeaders=host;x-amz-date, "
        "Signature=abcdef1234567890";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "AKIAIOSFODNN7EXAMPLE");
    EXPECT_EQ(result->credential_scope, "20220101/us-east-1/s3/aws4_request");
    EXPECT_EQ(result->signed_headers, "host;x-amz-date");
    EXPECT_EQ(result->signature, "abcdef1234567890");
    
    // Header with extra spaces
    header = 
        "AWS4-HMAC-SHA256 Credential= AKIAEXAMPLE/20220101/us-east-1/s3/aws4_request , "
        "SignedHeaders= host;x-amz-date , "
        "Signature= abcdef ";
    
    result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "AKIAEXAMPLE");
    EXPECT_EQ(result->credential_scope, "20220101/us-east-1/s3/aws4_request");
    EXPECT_EQ(result->signed_headers, "host;x-amz-date");
    EXPECT_EQ(result->signature, "abcdef");
    
    // Missing SignedHeaders (optional)
    header = 
        "AWS4-HMAC-SHA256 Credential=AKIAEXAMPLE/20220101/us-east-1/s3/aws4_request, "
        "Signature=abcdef";
    
    result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "AKIAEXAMPLE");
    EXPECT_EQ(result->credential_scope, "20220101/us-east-1/s3/aws4_request");
    EXPECT_TRUE(result->signed_headers.empty());
    EXPECT_EQ(result->signature, "abcdef");
    
    // Missing Signature (invalid)
    header = 
        "AWS4-HMAC-SHA256 Credential=AKIAEXAMPLE/20220101/us-east-1/s3/aws4_request, "
        "SignedHeaders=host;x-amz-date";
    
    result = authorization_header_parser::parse_aws4_header(header);
    EXPECT_FALSE(result.has_value());
    
    // Missing Credential (invalid)
    header = 
        "AWS4-HMAC-SHA256 SignedHeaders=host;x-amz-date, "
        "Signature=abcdef";
    
    result = authorization_header_parser::parse_aws4_header(header);
    EXPECT_FALSE(result.has_value());
    
    // Not AWS4 format
    result = authorization_header_parser::parse_aws4_header("Basic dXNlcjpwYXNz");
    EXPECT_FALSE(result.has_value());
}

TEST(AuthorizationHeaderParserTest, RealWorldExamples) {
    // Example from AWS documentation
    std::string header = 
        "AWS4-HMAC-SHA256 "
        "Credential=AKIAIOSFODNN7EXAMPLE/20130524/us-east-1/s3/aws4_request, "
        "SignedHeaders=host;range;x-amz-date, "
        "Signature=fe5f80f77d5fa3beca038a248ff027d0445342fe2855ddc963176630326f1024";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "AKIAIOSFODNN7EXAMPLE");
    EXPECT_EQ(result->credential_scope, "20130524/us-east-1/s3/aws4_request");
    EXPECT_EQ(result->signed_headers, "host;range;x-amz-date");
    EXPECT_EQ(result->signature, "fe5f80f77d5fa3beca038a248ff027d0445342fe2855ddc963176630326f1024");
    
    // Extract just access key ID
    auto access_key_id = authorization_header_parser::extract_access_key_id(header);
    ASSERT_TRUE(access_key_id.has_value());
    EXPECT_EQ(*access_key_id, "AKIAIOSFODNN7EXAMPLE");
}