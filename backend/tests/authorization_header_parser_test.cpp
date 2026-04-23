#include <gtest/gtest.h>
#include <chrono>
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


// ========== ОБРАБОТКА ПРОБЕЛОВ И ЗАПЯТЫХ ==========

TEST(AuthorizationHeaderParserTest, ParseHeader_WithExtraSpacesAroundCommas) {
    std::string header = 
        "AWS4-HMAC-SHA256   Credential=AKIAIOSFODNN7EXAMPLE/20220101/us-east-1/s3/aws4_request , "
        "SignedHeaders=host;x-amz-date  ,  Signature=abcdef1234567890";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "AKIAIOSFODNN7EXAMPLE");
    EXPECT_EQ(result->credential_scope, "20220101/us-east-1/s3/aws4_request");
    EXPECT_EQ(result->signed_headers, "host;x-amz-date");
    EXPECT_EQ(result->signature, "abcdef1234567890");
}

TEST(AuthorizationHeaderParserTest, ParseHeader_WithSpacesInCredential) {
    std::string header = 
        "AWS4-HMAC-SHA256 Credential= AKIAEXAMPLE / 20220101 / us-east-1 / s3 / aws4_request , "
        "SignedHeaders=host;x-amz-date, "
        "Signature=abcdef";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "AKIAEXAMPLE");
    EXPECT_EQ(result->credential_scope, "20220101 / us-east-1 / s3 / aws4_request");
    EXPECT_EQ(result->signed_headers, "host;x-amz-date");
    EXPECT_EQ(result->signature, "abcdef");
}

TEST(AuthorizationHeaderParserTest, ParseHeader_WithTabsAndNewlines) {
    std::string header = 
        "AWS4-HMAC-SHA256\tCredential=AKIAEXAMPLE/20220101/us-east-1/s3/aws4_request,\n"
        " SignedHeaders=host;x-amz-date,\r\n"
        " Signature=abcdef";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "AKIAEXAMPLE");
    EXPECT_EQ(result->credential_scope, "20220101/us-east-1/s3/aws4_request");
    EXPECT_EQ(result->signed_headers, "host;x-amz-date");
    EXPECT_EQ(result->signature, "abcdef");
}

// ========== ОТСУТСТВИЕ SIGNEDHEADERS ==========

TEST(AuthorizationHeaderParserTest, ParseHeader_MissingSignedHeaders) {
    // SignedHeaders не является обязательным полем в спецификации, но часто присутствует
    std::string header = 
        "AWS4-HMAC-SHA256 Credential=AKIAEXAMPLE/20220101/us-east-1/s3/aws4_request, "
        "Signature=abcdef1234567890";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "AKIAEXAMPLE");
    EXPECT_EQ(result->credential_scope, "20220101/us-east-1/s3/aws4_request");
    EXPECT_TRUE(result->signed_headers.empty());
    EXPECT_EQ(result->signature, "abcdef1234567890");
}

// ========== ИГНОРИРОВАНИЕ НЕИЗВЕСТНЫХ ПАРАМЕТРОВ ==========

TEST(AuthorizationHeaderParserTest, ParseHeader_ExtraParameters) {
    std::string header = 
        "AWS4-HMAC-SHA256 Credential=AKIAEXAMPLE/20220101/us-east-1/s3/aws4_request, "
        "SignedHeaders=host;x-amz-date, "
        "Signature=abcdef, "
        "ExtraParam=somevalue, "
        "AnotherParam=12345";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "AKIAEXAMPLE");
    EXPECT_EQ(result->credential_scope, "20220101/us-east-1/s3/aws4_request");
    EXPECT_EQ(result->signed_headers, "host;x-amz-date");
    EXPECT_EQ(result->signature, "abcdef");
    // Extra params should be ignored (query_params currently unused but could be stored)
}

TEST(AuthorizationHeaderParserTest, ParseHeader_ExtraParametersBeforeCredential) {
    // Некоторые реализации могут ставить параметры в другом порядке
    std::string header = 
        "AWS4-HMAC-SHA256 SignedHeaders=host;x-amz-date, "
        "Credential=AKIAEXAMPLE/20220101/us-east-1/s3/aws4_request, "
        "Signature=abcdef, "
        "Extra=value";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "AKIAEXAMPLE");
    EXPECT_EQ(result->credential_scope, "20220101/us-east-1/s3/aws4_request");
    EXPECT_EQ(result->signed_headers, "host;x-amz-date");
    EXPECT_EQ(result->signature, "abcdef");
}

// ========== ОБРАБОТКА ПУСТЫХ ЗНАЧЕНИЙ ==========

TEST(AuthorizationHeaderParserTest, ParseHeader_EmptyCredential) {
    std::string header = 
        "AWS4-HMAC-SHA256 Credential=, SignedHeaders=host, Signature=abc";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    EXPECT_FALSE(result.has_value());
}

TEST(AuthorizationHeaderParserTest, ParseHeader_EmptySignature) {
    std::string header = 
        "AWS4-HMAC-SHA256 Credential=AKIAEXAMPLE/20220101/us-east-1/s3/aws4_request, "
        "SignedHeaders=host, Signature=";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    EXPECT_FALSE(result.has_value());
}

TEST(AuthorizationHeaderParserTest, ParseHeader_MalformedCredential_NoSlash) {
    std::string header = 
        "AWS4-HMAC-SHA256 Credential=AKIAEXAMPLE, SignedHeaders=host, Signature=abc";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    EXPECT_FALSE(result.has_value());
}

// ========== РАЗЛИЧНЫЕ ФОРМАТЫ ПАРАМЕТРОВ ==========

TEST(AuthorizationHeaderParserTest, ParseHeader_WithoutSpacesBetweenParams) {
    std::string header = 
        "AWS4-HMAC-SHA256 Credential=AKIAEXAMPLE/20220101/us-east-1/s3/aws4_request,SignedHeaders=host;x-amz-date,Signature=abcdef";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "AKIAEXAMPLE");
    EXPECT_EQ(result->credential_scope, "20220101/us-east-1/s3/aws4_request");
    EXPECT_EQ(result->signed_headers, "host;x-amz-date");
    EXPECT_EQ(result->signature, "abcdef");
}

TEST(AuthorizationHeaderParserTest, ParseHeader_WithQuotedValues) {
    // Хотя спецификация AWS не использует кавычки, парсер должен корректно обрабатывать
    std::string header = 
        "AWS4-HMAC-SHA256 Credential=\"AKIAEXAMPLE/20220101/us-east-1/s3/aws4_request\", "
        "SignedHeaders=\"host;x-amz-date\", "
        "Signature=\"abcdef\"";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "AKIAEXAMPLE");
    EXPECT_EQ(result->credential_scope, "20220101/us-east-1/s3/aws4_request");
    EXPECT_EQ(result->signed_headers, "host;x-amz-date");
    EXPECT_EQ(result->signature, "abcdef");
}

// ========== ТЕСТЫ ПРОИЗВОДИТЕЛЬНОСТИ EXTRACT_ACCESS_KEY_ID ==========

TEST(AuthorizationHeaderParserTest, ExtractAccessKeyId_Performance) {
    // Создаем типичный заголовок
    std::string header = 
        "AWS4-HMAC-SHA256 Credential=AKIAIOSFODNN7EXAMPLE/20220101/us-east-1/s3/aws4_request, "
        "SignedHeaders=host;x-amz-date, "
        "Signature=fe5f80f77d5fa3beca038a248ff027d0445342fe2855ddc963176630326f1024";
    
    const int iterations = 100000;
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        auto result = authorization_header_parser::extract_access_key_id(header);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, "AKIAIOSFODNN7EXAMPLE");
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // В среднем должно быть быстрее полного парсинга (но это просто информативно)
    std::cout << "[PERF] extract_access_key_id: " << duration << " us for " << iterations << " iterations ("
              << (duration / iterations) << " us/op)" << std::endl;
    
    // Сравним с полным парсингом
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        auto result = authorization_header_parser::parse_aws4_header(header);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->access_key_id, "AKIAIOSFODNN7EXAMPLE");
    }
    end = std::chrono::high_resolution_clock::now();
    auto duration_full = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    std::cout << "[PERF] parse_aws4_header: " << duration_full << " us for " << iterations << " iterations ("
              << (duration_full / iterations) << " us/op)" << std::endl;
    
    // extract_access_key_id должен быть быстрее
    EXPECT_LT(duration, duration_full);
}

// ========== РЕАЛЬНЫЕ ПРИМЕРЫ С РАЗНЫМИ СЕРВИСАМИ ==========

TEST(AuthorizationHeaderParserTest, ParseHeader_FromS3GetObject) {
    // Пример из реального запроса к S3
    std::string header = 
        "AWS4-HMAC-SHA256 "
        "Credential=AKIAIOSFODNN7EXAMPLE/20220101/us-east-1/s3/aws4_request, "
        "SignedHeaders=host;x-amz-content-sha256;x-amz-date, "
        "Signature=9f08f5f3f8e5c6c0a9b8d7e6f5a4c3b2e1d0c9b8a7f6e5d4c3b2a1";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "AKIAIOSFODNN7EXAMPLE");
    EXPECT_EQ(result->credential_scope, "20220101/us-east-1/s3/aws4_request");
    EXPECT_EQ(result->signed_headers, "host;x-amz-content-sha256;x-amz-date");
    EXPECT_EQ(result->signature, "9f08f5f3f8e5c6c0a9b8d7e6f5a4c3b2e1d0c9b8a7f6e5d4c3b2a1");
}

TEST(AuthorizationHeaderParserTest, ParseHeader_FromSTS) {
    // Пример для AWS Security Token Service
    std::string header = 
        "AWS4-HMAC-SHA256 "
        "Credential=ASIAEXAMPLE/20220101/us-east-1/sts/aws4_request, "
        "SignedHeaders=content-type;host;x-amz-date, "
        "Signature=7f8e5d4c3b2a1f6e5d4c3b2a1f6e5d4c3b2a1f6e5d4c3b2a1f6e5d4c3b2a1";
    
    auto result = authorization_header_parser::parse_aws4_header(header);
    
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->access_key_id, "ASIAEXAMPLE");
    EXPECT_EQ(result->credential_scope, "20220101/us-east-1/sts/aws4_request");
    EXPECT_EQ(result->signed_headers, "content-type;host;x-amz-date");
}

// ========== НЕГАТИВНЫЕ ТЕСТЫ ==========

TEST(AuthorizationHeaderParserTest, IsAws4Format_EdgeCases) {
    EXPECT_FALSE(authorization_header_parser::is_aws4_format("aws4-hmac-sha256 ...")); // lower case
    EXPECT_FALSE(authorization_header_parser::is_aws4_format(" AWS4-HMAC-SHA256 ...")); // leading space
    EXPECT_FALSE(authorization_header_parser::is_aws4_format("AWS4-HMAC-SHA1 ...")); // wrong algorithm
    EXPECT_TRUE(authorization_header_parser::is_aws4_format("AWS4-HMAC-SHA256")); // minimal
}

TEST(AuthorizationHeaderParserTest, ExtractAccessKeyId_FromEmptyString) {
    auto result = authorization_header_parser::extract_access_key_id("");
    EXPECT_FALSE(result.has_value());
}

TEST(AuthorizationHeaderParserTest, ExtractAccessKeyId_FromNonAws4) {
    auto result = authorization_header_parser::extract_access_key_id("Basic dXNlcjpwYXNz");
    EXPECT_FALSE(result.has_value());
}

TEST(AuthorizationHeaderParserTest, ParseCredential_WithTrailingSlash) {
    auto [ak, scope] = authorization_header_parser::parse_credential("AKIAEXAMPLE/");
    EXPECT_EQ(ak, "AKIAEXAMPLE");
    EXPECT_EQ(scope, "");
}

TEST(AuthorizationHeaderParserTest, ParseCredential_WithMultipleSlashes) {
    auto [ak, scope] = authorization_header_parser::parse_credential("AKIAEXAMPLE/20220101/us-east-1//s3/aws4_request");
    EXPECT_EQ(ak, "AKIAEXAMPLE");
    EXPECT_EQ(scope, "20220101/us-east-1//s3/aws4_request");
}
