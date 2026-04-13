#include <gtest/gtest.h>
#include "authenticator.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <random>

namespace fs = std::filesystem;

class AuthenticatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary directory for test files
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis;
        uint64_t random = dis(gen);
        _temp_dir = fs::temp_directory_path() / ("auth_test_" + std::to_string(random));
        fs::create_directories(_temp_dir);
        
        _keys_file = _temp_dir / "access_keys.csv";
    }
    
    void TearDown() override {
        // Remove temporary directory
        if (fs::exists(_temp_dir)) {
            fs::remove_all(_temp_dir);
        }
    }
    
    // Helper to create a CSV file with given content
    void create_csv(const std::string& content) {
        std::ofstream file(_keys_file);
        file << content;
        file.close();
    }
    
    // Helper to read CSV file content
    std::string read_csv() {
        std::ifstream file(_keys_file);
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    fs::path _temp_dir;
    fs::path _keys_file;
};

// Test loading keys from a valid CSV file
TEST_F(AuthenticatorTest, LoadKeysValid) {
    authenticator auth;
    
    // Create a CSV with one key
    create_csv("AKIAEXAMPLE,secret123,user1,1\n");
    
    bool loaded = auth.load_keys(_keys_file.string());
    EXPECT_TRUE(loaded);
    
    auto key = auth.get_key("AKIAEXAMPLE");
    ASSERT_TRUE(key.has_value());
    EXPECT_EQ(key->access_key_id, "AKIAEXAMPLE");
    EXPECT_EQ(key->secret_access_key, "secret123");
    EXPECT_EQ(key->user_name, "user1");
    EXPECT_TRUE(key->is_active);
}

// Test loading keys from non-existent file
TEST_F(AuthenticatorTest, LoadKeysFileNotFound) {
    authenticator auth;
    bool loaded = auth.load_keys("nonexistent.csv");
    EXPECT_FALSE(loaded);
}

// Test loading keys with malformed CSV (missing fields)
TEST_F(AuthenticatorTest, LoadKeysMalformed) {
    authenticator auth;
    create_csv("AKIAEXAMPLE,secret123\n"); // missing user_name and is_active
    bool loaded = auth.load_keys(_keys_file.string());
    // According to implementation, malformed lines are skipped, but load returns true if file exists
    // Let's see: the implementation expects 4 fields, if not enough it skips line.
    // The function returns true if file opened successfully.
    EXPECT_TRUE(loaded);
    // No keys should be loaded
    auto key = auth.get_key("AKIAEXAMPLE");
    EXPECT_FALSE(key.has_value());
}

// Test saving keys to file
TEST_F(AuthenticatorTest, SaveKeys) {
    authenticator auth;
    
    // Create a key via authenticator (since we don't have direct add method)
    auto key = auth.create_access_key("testuser");
    ASSERT_TRUE(key.has_value());
    
    bool saved = auth.save_keys(_keys_file.string());
    EXPECT_TRUE(saved);
    
    // Verify file content
    std::string content = read_csv();
    EXPECT_NE(content.find(key->access_key_id), std::string::npos);
    EXPECT_NE(content.find(key->secret_access_key), std::string::npos);
    EXPECT_NE(content.find("testuser"), std::string::npos);
    EXPECT_NE(content.find("1"), std::string::npos); // active flag
}

// Test saving and then loading back
TEST_F(AuthenticatorTest, SaveAndLoadRoundtrip) {
    authenticator auth1;
    auto key = auth1.create_access_key("roundtrip_user");
    ASSERT_TRUE(key.has_value());
    
    bool saved = auth1.save_keys(_keys_file.string());
    ASSERT_TRUE(saved);
    
    authenticator auth2;
    bool loaded = auth2.load_keys(_keys_file.string());
    EXPECT_TRUE(loaded);
    
    auto loaded_key = auth2.get_key(key->access_key_id);
    ASSERT_TRUE(loaded_key.has_value());
    EXPECT_EQ(loaded_key->access_key_id, key->access_key_id);
    EXPECT_EQ(loaded_key->secret_access_key, key->secret_access_key);
    EXPECT_EQ(loaded_key->user_name, key->user_name);
    EXPECT_EQ(loaded_key->is_active, key->is_active);
}

// Test loading multiple keys
TEST_F(AuthenticatorTest, LoadMultipleKeys) {
    authenticator auth;
    create_csv("AKIA1,secret1,user1,1\n"
               "AKIA2,secret2,user2,0\n"
               "AKIA3,secret3,user3,1\n");
    
    bool loaded = auth.load_keys(_keys_file.string());
    EXPECT_TRUE(loaded);
    
    auto key1 = auth.get_key("AKIA1");
    auto key2 = auth.get_key("AKIA2");
    auto key3 = auth.get_key("AKIA3");
    
    ASSERT_TRUE(key1.has_value());
    ASSERT_TRUE(key2.has_value());
    ASSERT_TRUE(key3.has_value());
    
    EXPECT_EQ(key1->user_name, "user1");
    EXPECT_TRUE(key1->is_active);
    EXPECT_EQ(key2->user_name, "user2");
    EXPECT_FALSE(key2->is_active);
    EXPECT_EQ(key3->user_name, "user3");
    EXPECT_TRUE(key3->is_active);
}

// Test create_access_key generates unique IDs
TEST_F(AuthenticatorTest, CreateAccessKey) {
    authenticator auth;
    auto key1 = auth.create_access_key("user1");
    auto key2 = auth.create_access_key("user2");
    
    ASSERT_TRUE(key1.has_value());
    ASSERT_TRUE(key2.has_value());
    
    EXPECT_EQ(key1->user_name, "user1");
    EXPECT_EQ(key2->user_name, "user2");
    EXPECT_TRUE(key1->is_active);
    EXPECT_TRUE(key2->is_active);
    EXPECT_NE(key1->access_key_id, key2->access_key_id);
    EXPECT_NE(key1->secret_access_key, key2->secret_access_key);
    
    // Verify keys can be retrieved
    auto retrieved = auth.get_key(key1->access_key_id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->secret_access_key, key1->secret_access_key);
}

// Test create_access_key with custom ID
TEST_F(AuthenticatorTest, CreateAccessKeyCustomId) {
    authenticator auth;
    auto key = auth.create_access_key("user", "CUSTOMID123");
    ASSERT_TRUE(key.has_value());
    EXPECT_EQ(key->access_key_id, "CUSTOMID123");
    EXPECT_EQ(key->user_name, "user");
}

// Test delete_access_key
TEST_F(AuthenticatorTest, DeleteAccessKey) {
    authenticator auth;
    auto key = auth.create_access_key("user");
    ASSERT_TRUE(key.has_value());
    
    bool deleted = auth.delete_access_key(key->access_key_id);
    EXPECT_TRUE(deleted);
    
    auto retrieved = auth.get_key(key->access_key_id);
    EXPECT_FALSE(retrieved.has_value());
}

// Test delete_access_key non-existent
TEST_F(AuthenticatorTest, DeleteAccessKeyNotFound) {
    authenticator auth;
    bool deleted = auth.delete_access_key("NONEXISTENT");
    EXPECT_FALSE(deleted);
}

// Test activate/deactivate key
TEST_F(AuthenticatorTest, ActivateDeactivateKey) {
    authenticator auth;
    auto key = auth.create_access_key("user");
    ASSERT_TRUE(key.has_value());
    EXPECT_TRUE(key->is_active);
    
    // Deactivate
    bool deactivated = auth.deactivate_key(key->access_key_id);
    EXPECT_TRUE(deactivated);
    auto key2 = auth.get_key(key->access_key_id);
    ASSERT_TRUE(key2.has_value());
    EXPECT_FALSE(key2->is_active);
    
    // Activate again
    bool activated = auth.activate_key(key->access_key_id);
    EXPECT_TRUE(activated);
    auto key3 = auth.get_key(key->access_key_id);
    ASSERT_TRUE(key3.has_value());
    EXPECT_TRUE(key3->is_active);
}

// Test list_keys
TEST_F(AuthenticatorTest, ListKeys) {
    authenticator auth;
    auto key1 = auth.create_access_key("user1");
    auto key2 = auth.create_access_key("user2");
    auto key3 = auth.create_access_key("user3");
    
    std::vector<access_key> keys = auth.list_keys();
    EXPECT_EQ(keys.size(), 3);
    
    // Check that all keys are present
    std::set<std::string> ids;
    for (const auto& k : keys) {
        ids.insert(k.access_key_id);
    }
    EXPECT_TRUE(ids.count(key1->access_key_id) > 0);
    EXPECT_TRUE(ids.count(key2->access_key_id) > 0);
    EXPECT_TRUE(ids.count(key3->access_key_id) > 0);
}

// Test get_key non-existent
TEST_F(AuthenticatorTest, GetKeyNotFound) {
    authenticator auth;
    auto key = auth.get_key("NONEXISTENT");
    EXPECT_FALSE(key.has_value());
}

// Test verify_signature missing authorization header
TEST_F(AuthenticatorTest, VerifySignatureMissingAuthHeader) {
    authenticator auth;
    std::map<std::string, std::string> headers = {
        {"x-amz-date", "20220101T120000Z"}
    };
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    EXPECT_FALSE(verified);
}

// Test verify_signature with malformed authorization header
TEST_F(AuthenticatorTest, VerifySignatureMalformedAuthHeader) {
    authenticator auth;
    std::map<std::string, std::string> headers = {
        {"authorization", "InvalidHeader"},
        {"x-amz-date", "20220101T120000Z"}
    };
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    EXPECT_FALSE(verified);
}

// Test verify_signature with non-existent key
TEST_F(AuthenticatorTest, VerifySignatureKeyNotFound) {
    authenticator auth;
    std::map<std::string, std::string> headers = {
        {"authorization", "AWS4-HMAC-SHA256 Credential=AKIAEXAMPLE/20220101/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=abcdef"},
        {"x-amz-date", "20220101T120000Z"}
    };
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    EXPECT_FALSE(verified);
}

// Test verify_signature with inactive key
TEST_F(AuthenticatorTest, VerifySignatureInactiveKey) {
    authenticator auth;
    auto key = auth.create_access_key("user");
    ASSERT_TRUE(key.has_value());
    auth.deactivate_key(key->access_key_id);
    
    std::map<std::string, std::string> headers = {
        {"authorization", "AWS4-HMAC-SHA256 Credential=" + key->access_key_id + "/20220101/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=abcdef"},
        {"x-amz-date", "20220101T120000Z"}
    };
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    EXPECT_FALSE(verified);
}

// Test verify_signature with invalid timestamp
TEST_F(AuthenticatorTest, VerifySignatureInvalidTimestamp) {
    authenticator auth;
    auto key = auth.create_access_key("user");
    ASSERT_TRUE(key.has_value());
    
    // Timestamp far in the past (or missing)
    std::map<std::string, std::string> headers = {
        {"authorization", "AWS4-HMAC-SHA256 Credential=" + key->access_key_id + "/20220101/us-east-1/s3/aws4_request, SignedHeaders=host;x-amz-date, Signature=abcdef"},
        {"x-amz-date", "19990101T120000Z"} // old timestamp
    };
    bool verified = auth.verify_signature("GET", "/test", headers, "");
    EXPECT_FALSE(verified);
}

// Test generate_signature returns empty string (not implemented)
// TEST_F(AuthenticatorTest, GenerateSignatureNotImplemented) {
//     authenticator auth;
//     std::map<std::string, std::string> headers = {{"x-amz-date", "20220101T120000Z"}};
//     std::string sig = auth.generate_signature("AKIAEXAMPLE", "secret", "GET", "/test", headers, "");
//     EXPECT_EQ(sig, "");
// }