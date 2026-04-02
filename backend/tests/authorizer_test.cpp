#include <gtest/gtest.h>
#include "authorizer.hpp"
#include <chrono>

class AuthorizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Each test starts with a fresh authorizer instance
        auth = std::make_unique<authorizer>();
    }

    void TearDown() override {
        auth.reset();
    }

    std::unique_ptr<authorizer> auth;
};

// Test constructor and initial state
TEST_F(AuthorizerTest, InitialState) {
    // Should have no users, policies, or ACLs
    EXPECT_EQ(auth->list_users().size(), 0);
    EXPECT_EQ(auth->list_policies().size(), 0);
}

// ========== User Management Tests ==========

TEST_F(AuthorizerTest, CreateUserSuccess) {
    auto user = auth->create_user("alice", "alice@example.com", user_role::VIEWER);
    ASSERT_TRUE(user.has_value());
    EXPECT_EQ(user->username, "alice");
    EXPECT_EQ(user->email, "alice@example.com");
    EXPECT_EQ(user->role, user_role::VIEWER);
    EXPECT_TRUE(user->is_active);
    EXPECT_FALSE(user->user_id.empty());

    // Should be able to retrieve by ID
    auto retrieved = auth->get_user(user->user_id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->username, "alice");
}

TEST_F(AuthorizerTest, CreateUserDuplicateUsername) {
    auto user1 = auth->create_user("alice", "alice@example.com");
    ASSERT_TRUE(user1.has_value());

    auto user2 = auth->create_user("alice", "another@example.com");
    EXPECT_FALSE(user2.has_value()); // Should fail
}

TEST_F(AuthorizerTest, GetUserNotFound) {
    auto user = auth->get_user("nonexistent");
    EXPECT_FALSE(user.has_value());
}

TEST_F(AuthorizerTest, GetUserByName) {
    auto created = auth->create_user("bob", "bob@example.com");
    ASSERT_TRUE(created.has_value());

    auto found = auth->get_user_by_name("bob");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->user_id, created->user_id);
}

TEST_F(AuthorizerTest, UpdateUserRole) {
    auto user = auth->create_user("charlie", "charlie@example.com", user_role::VIEWER);
    ASSERT_TRUE(user.has_value());

    bool updated = auth->update_user_role(user->user_id, user_role::ADMIN);
    EXPECT_TRUE(updated);

    auto retrieved = auth->get_user(user->user_id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->role, user_role::ADMIN);
}

TEST_F(AuthorizerTest, UpdateUserRoleNotFound) {
    bool updated = auth->update_user_role("nonexistent", user_role::ADMIN);
    EXPECT_FALSE(updated);
}

TEST_F(AuthorizerTest, ActivateDeactivateUser) {
    auto user = auth->create_user("dave", "dave@example.com");
    ASSERT_TRUE(user.has_value());

    // Initially active
    auto retrieved = auth->get_user(user->user_id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_TRUE(retrieved->is_active);

    // Deactivate
    bool deactivated = auth->deactivate_user(user->user_id);
    EXPECT_TRUE(deactivated);
    retrieved = auth->get_user(user->user_id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_FALSE(retrieved->is_active);

    // Activate
    bool activated = auth->activate_user(user->user_id);
    EXPECT_TRUE(activated);
    retrieved = auth->get_user(user->user_id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_TRUE(retrieved->is_active);
}

TEST_F(AuthorizerTest, DeleteUser) {
    auto user = auth->create_user("eve", "eve@example.com");
    ASSERT_TRUE(user.has_value());

    bool deleted = auth->delete_user(user->user_id);
    EXPECT_TRUE(deleted);

    auto retrieved = auth->get_user(user->user_id);
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(AuthorizerTest, DeleteUserNotFound) {
    bool deleted = auth->delete_user("nonexistent");
    EXPECT_FALSE(deleted);
}

TEST_F(AuthorizerTest, ListUsers) {
    EXPECT_EQ(auth->list_users().size(), 0);

    auto user1 = auth->create_user("user1", "user1@example.com");
    auto user2 = auth->create_user("user2", "user2@example.com");
    ASSERT_TRUE(user1.has_value());
    ASSERT_TRUE(user2.has_value());

    auto users = auth->list_users();
    EXPECT_EQ(users.size(), 2);
}

// ========== Policy Management Tests ==========

TEST_F(AuthorizerTest, CreatePolicySuccess) {
    std::vector<permission> perms = {
        {permission_type::READ, "bucket/*", true},
        {permission_type::WRITE, "bucket/private/*", false}
    };

    auto policy = auth->create_policy("TestPolicy", "A test policy", perms);
    ASSERT_TRUE(policy.has_value());
    EXPECT_EQ(policy->name, "TestPolicy");
    EXPECT_EQ(policy->description, "A test policy");
    EXPECT_EQ(policy->permissions.size(), 2);
    EXPECT_FALSE(policy->policy_id.empty());

    // Retrieve
    auto retrieved = auth->get_policy(policy->policy_id);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, "TestPolicy");
}

TEST_F(AuthorizerTest, GetPolicyNotFound) {
    auto policy = auth->get_policy("nonexistent");
    EXPECT_FALSE(policy.has_value());
}

TEST_F(AuthorizerTest, DeletePolicy) {
    std::vector<permission> perms = {{permission_type::READ, "*", true}};
    auto policy = auth->create_policy("ToDelete", "Will be deleted", perms);
    ASSERT_TRUE(policy.has_value());

    bool deleted = auth->delete_policy(policy->policy_id);
    EXPECT_TRUE(deleted);

    auto retrieved = auth->get_policy(policy->policy_id);
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(AuthorizerTest, ListPolicies) {
    EXPECT_EQ(auth->list_policies().size(), 0);

    std::vector<permission> perms = {{permission_type::READ, "*", true}};
    auth->create_policy("Policy1", "First", perms);
    auth->create_policy("Policy2", "Second", perms);

    auto policies = auth->list_policies();
    EXPECT_EQ(policies.size(), 2);
}

// ========== Resource ACL Tests ==========

TEST_F(AuthorizerTest, SetAndGetResourceACL) {
    resource_acl acl;
    acl.resource_path = "/bucket/file.txt";
    acl.is_public = false;
    acl.owner_user_id = "user123";
    acl.user_permissions["user456"] = {permission_type::READ, permission_type::WRITE};

    bool set = auth->set_resource_acl("/bucket/file.txt", acl);
    EXPECT_TRUE(set);

    auto retrieved = auth->get_resource_acl("/bucket/file.txt");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->resource_path, "/bucket/file.txt");
    EXPECT_EQ(retrieved->is_public, false);
    EXPECT_EQ(*retrieved->owner_user_id, "user123");
    EXPECT_EQ(retrieved->user_permissions.size(), 1);
    EXPECT_EQ(retrieved->user_permissions["user456"].size(), 2);
}

TEST_F(AuthorizerTest, GetResourceACLInheritance) {
    // Set ACL for parent directory
    resource_acl parent_acl;
    parent_acl.resource_path = "/bucket";
    parent_acl.is_public = true;
    auth->set_resource_acl("/bucket", parent_acl);

    // Child should inherit
    auto child_acl = auth->get_resource_acl("/bucket/sub/file.txt");
    ASSERT_TRUE(child_acl.has_value());
    EXPECT_EQ(child_acl->resource_path, "/bucket");
    EXPECT_TRUE(child_acl->is_public);
}

TEST_F(AuthorizerTest, RemoveResourceACL) {
    resource_acl acl;
    acl.resource_path = "/test";
    auth->set_resource_acl("/test", acl);

    bool removed = auth->remove_resource_acl("/test");
    EXPECT_TRUE(removed);

    auto retrieved = auth->get_resource_acl("/test");
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(AuthorizerTest, MakeResourcePublicPrivate) {
    // Make public
    bool pub = auth->make_resource_public("/public.txt");
    EXPECT_TRUE(pub);

    auto acl = auth->get_resource_acl("/public.txt");
    ASSERT_TRUE(acl.has_value());
    EXPECT_TRUE(acl->is_public);

    // Make private
    bool priv = auth->make_resource_private("/public.txt");
    EXPECT_TRUE(priv);

    acl = auth->get_resource_acl("/public.txt");
    ASSERT_TRUE(acl.has_value());
    EXPECT_FALSE(acl->is_public);
}

TEST_F(AuthorizerTest, AddUserPermission) {
    bool added = auth->add_user_permission("/res", "user1", permission_type::READ);
    EXPECT_TRUE(added);

    auto acl = auth->get_resource_acl("/res");
    ASSERT_TRUE(acl.has_value());
    EXPECT_EQ(acl->user_permissions["user1"].count(permission_type::READ), 1);
}

TEST_F(AuthorizerTest, RemoveUserPermission) {
    auth->add_user_permission("/res", "user1", permission_type::READ);
    auth->add_user_permission("/res", "user1", permission_type::WRITE);

    bool removed = auth->remove_user_permission("/res", "user1", permission_type::READ);
    EXPECT_TRUE(removed);

    auto acl = auth->get_resource_acl("/res");
    ASSERT_TRUE(acl.has_value());
    EXPECT_EQ(acl->user_permissions["user1"].count(permission_type::READ), 0);
    EXPECT_EQ(acl->user_permissions["user1"].count(permission_type::WRITE), 1);

    // Remove non-existent permission
    bool removed2 = auth->remove_user_permission("/res", "user1", permission_type::DELETE);
    EXPECT_FALSE(removed2);
}

TEST_F(AuthorizerTest, AddGroupPermission) {
    bool added = auth->add_group_permission("/res", "developers", permission_type::WRITE);
    EXPECT_TRUE(added);

    auto acl = auth->get_resource_acl("/res");
    ASSERT_TRUE(acl.has_value());
    EXPECT_EQ(acl->group_permissions["developers"].count(permission_type::WRITE), 1);
}

// ========== Access Checking Tests ==========

TEST_F(AuthorizerTest, CheckAccessAdmin) {
    auto admin = auth->create_user("admin", "admin@example.com", user_role::ADMIN);
    ASSERT_TRUE(admin.has_value());

    bool access = auth->check_access(admin->user_id, "/any/resource", permission_type::DELETE);
    EXPECT_TRUE(access); // Admin has full access
}

TEST_F(AuthorizerTest, CheckAccessByRole) {
    auto viewer = auth->create_user("viewer", "viewer@example.com", user_role::VIEWER);
    ASSERT_TRUE(viewer.has_value());

    // Viewer has READ and LIST permissions
    bool can_read = auth->check_access(viewer->user_id, "/resource", permission_type::READ);
    EXPECT_TRUE(can_read);

    bool can_write = auth->check_access(viewer->user_id, "/resource", permission_type::WRITE);
    EXPECT_FALSE(can_write); // Viewer cannot write
}

TEST_F(AuthorizerTest, CheckAccessByACL) {
    auto user = auth->create_user("user", "user@example.com", user_role::VIEWER);
    ASSERT_TRUE(user.has_value());

    // Grant WRITE permission via ACL
    auth->add_user_permission("/specific", user->user_id, permission_type::WRITE);

    bool can_write = auth->check_access(user->user_id, "/specific", permission_type::WRITE);
    EXPECT_TRUE(can_write);
}

TEST_F(AuthorizerTest, CheckAccessInactiveUser) {
    auto user = auth->create_user("inactive", "inactive@example.com", user_role::VIEWER);
    ASSERT_TRUE(user.has_value());
    auth->deactivate_user(user->user_id);

    bool access = auth->check_access(user->user_id, "/resource", permission_type::READ);
    EXPECT_FALSE(access);
}

TEST_F(AuthorizerTest, CheckPublicAccess) {
    auth->make_resource_public("/public.txt");

    bool public_read = auth->check_public_access("/public.txt", permission_type::READ);
    EXPECT_TRUE(public_read);

    bool public_write = auth->check_public_access("/public.txt", permission_type::WRITE);
    EXPECT_FALSE(public_write); // Public only allows READ and LIST

    bool private_access = auth->check_public_access("/private.txt", permission_type::READ);
    EXPECT_FALSE(private_access);
}

TEST_F(AuthorizerTest, IsAdmin) {
    auto admin = auth->create_user("admin", "admin@example.com", user_role::ADMIN);
    auto viewer = auth->create_user("viewer", "viewer@example.com", user_role::VIEWER);
    ASSERT_TRUE(admin.has_value());
    ASSERT_TRUE(viewer.has_value());

    EXPECT_TRUE(auth->is_admin(admin->user_id));
    EXPECT_FALSE(auth->is_admin(viewer->user_id));
    EXPECT_FALSE(auth->is_admin("nonexistent"));
}

TEST_F(AuthorizerTest, IsResourceOwner) {
    auto owner = auth->create_user("owner", "owner@example.com");
    ASSERT_TRUE(owner.has_value());

    resource_acl acl;
    acl.resource_path = "/owned";
    acl.owner_user_id = owner->user_id;
    auth->set_resource_acl("/owned", acl);

    EXPECT_TRUE(auth->is_resource_owner(owner->user_id, "/owned"));
    EXPECT_FALSE(auth->is_resource_owner("other", "/owned"));
}

// ========== Static Utility Tests ==========

TEST(AuthorizerStaticTest, ParsePermission) {
    EXPECT_EQ(authorizer::parse_permission("read"), permission_type::READ);
    EXPECT_EQ(authorizer::parse_permission("WRITE"), permission_type::WRITE);
    EXPECT_EQ(authorizer::parse_permission("DELETE"), permission_type::DELETE);
    EXPECT_EQ(authorizer::parse_permission("list"), permission_type::LIST);
    EXPECT_EQ(authorizer::parse_permission("MANAGE_ACL"), permission_type::MANAGE_ACL);
    EXPECT_FALSE(authorizer::parse_permission("invalid").has_value());
}

TEST(AuthorizerStaticTest, PermissionToString) {
    EXPECT_EQ(authorizer::permission_to_string(permission_type::READ), "READ");
    EXPECT_EQ(authorizer::permission_to_string(permission_type::WRITE), "WRITE");
    EXPECT_EQ(authorizer::permission_to_string(permission_type::DELETE), "DELETE");
    EXPECT_EQ(authorizer::permission_to_string(permission_type::LIST), "LIST");
    EXPECT_EQ(authorizer::permission_to_string(permission_type::MANAGE_ACL), "MANAGE_ACL");
}

TEST(AuthorizerStaticTest, ParseRole) {
    EXPECT_EQ(authorizer::parse_role("admin"), user_role::ADMIN);
    EXPECT_EQ(authorizer::parse_role("MANAGER"), user_role::MANAGER);
    EXPECT_EQ(authorizer::parse_role("contributor"), user_role::CONTRIBUTOR);
    EXPECT_EQ(authorizer::parse_role("viewer"), user_role::VIEWER);
    EXPECT_EQ(authorizer::parse_role("guest"), user_role::GUEST);
    EXPECT_FALSE(authorizer::parse_role("invalid").has_value());
}

TEST(AuthorizerStaticTest, RoleToString) {
    EXPECT_EQ(authorizer::role_to_string(user_role::ADMIN), "ADMIN");
    EXPECT_EQ(authorizer::role_to_string(user_role::MANAGER), "MANAGER");
    EXPECT_EQ(authorizer::role_to_string(user_role::CONTRIBUTOR), "CONTRIBUTOR");
    EXPECT_EQ(authorizer::role_to_string(user_role::VIEWER), "VIEWER");
    EXPECT_EQ(authorizer::role_to_string(user_role::GUEST), "GUEST");
}

TEST(AuthorizerStaticTest, MatchesPattern) {
    EXPECT_TRUE(authorizer::matches_pattern("/bucket/file.txt", "/bucket/*"));
    EXPECT_TRUE(authorizer::matches_pattern("/bucket/sub/file.txt", "/bucket/*"));
    EXPECT_FALSE(authorizer::matches_pattern("/other/file.txt", "/bucket/*"));
    EXPECT_TRUE(authorizer::matches_pattern("/any", "*"));
    EXPECT_TRUE(authorizer::matches_pattern("/any/path", "*"));
    // Edge cases
    EXPECT_TRUE(authorizer::matches_pattern("", "*")); // empty path matches wildcard
    EXPECT_TRUE(authorizer::matches_pattern("", "")); // empty pattern matches empty path
}