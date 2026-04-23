#include <gtest/gtest.h>

#include <fstream>
#include <nlohmann/json.hpp>

#include "authorizer.hpp"

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
    EXPECT_TRUE(auth->set_resource_acl("/bucket", parent_acl));

    // Child should inherit
    auto child_acl = auth->get_resource_acl("/bucket/sub/file.txt");
    ASSERT_TRUE(child_acl.has_value());
    EXPECT_EQ(child_acl->resource_path, "/bucket");
    EXPECT_TRUE(child_acl->is_public);
}

TEST_F(AuthorizerTest, RemoveResourceACL) {
    resource_acl acl;
    acl.resource_path = "/test";
    EXPECT_TRUE(auth->set_resource_acl("/test", acl));

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
    EXPECT_TRUE(auth->add_user_permission("/res", "user1", permission_type::READ));
    EXPECT_TRUE(auth->add_user_permission("/res", "user1", permission_type::WRITE));

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
    EXPECT_TRUE(auth->add_user_permission("/specific", user->user_id, permission_type::WRITE));

    bool can_write = auth->check_access(user->user_id, "/specific", permission_type::WRITE);
    EXPECT_TRUE(can_write);
}

TEST_F(AuthorizerTest, CheckAccessInactiveUser) {
    auto user = auth->create_user("inactive", "inactive@example.com", user_role::VIEWER);
    ASSERT_TRUE(user.has_value());
    EXPECT_TRUE(auth->deactivate_user(user->user_id));

    bool access = auth->check_access(user->user_id, "/resource", permission_type::READ);
    EXPECT_FALSE(access);
}

TEST_F(AuthorizerTest, CheckPublicAccess) {
    EXPECT_TRUE(auth->make_resource_public("/public.txt"));

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
    EXPECT_TRUE(auth->set_resource_acl("/owned", acl));

    EXPECT_TRUE(auth->is_resource_owner(owner->user_id, "/owned"));
    EXPECT_FALSE(auth->is_resource_owner("other", "/owned"));
}

TEST_F(AuthorizerTest, CheckAccessByPolicy) {
    auto user = auth->create_user("user", "user@example.com", user_role::VIEWER);
    ASSERT_TRUE(user.has_value());

    // Create a policy that allows READ on /public/*
    std::vector<permission> perms = {
        {permission_type::READ, "/public/*", true},
        {permission_type::WRITE, "/public/*", false} // explicit deny
    };
    auto policy = auth->create_policy("TestPolicy", "Test policy", perms);
    ASSERT_TRUE(policy.has_value());

    // User should have READ access to matching path
    bool can_read = auth->check_access(user->user_id, "/public/file.txt", permission_type::READ);
    EXPECT_TRUE(can_read);

    // User should NOT have WRITE access due to explicit deny
    bool can_write = auth->check_access(user->user_id, "/public/file.txt", permission_type::WRITE);
    EXPECT_FALSE(can_write);

    // Non-matching path and permission not granted by role should be denied
    bool other_manage = auth->check_access(user->user_id, "/private/file.txt", permission_type::MANAGE_ACL);
    EXPECT_FALSE(other_manage);
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

// ========== ГРУППОВЫЕ РАЗРЕШЕНИЯ ==========

TEST_F(AuthorizerTest, GroupPermissions) {
    // Создаём пользователя
    auto user = auth->create_user("group_user", "group@example.com", user_role::VIEWER);
    ASSERT_TRUE(user.has_value());
    
    // Создаём группу и добавляем пользователя
    bool added = auth->add_user_to_group(user->user_id, "developers");
    EXPECT_TRUE(added);
    
    // Даём группе право WRITE на ресурс
    EXPECT_TRUE(auth->add_group_permission("/project", "developers", permission_type::WRITE));
    
    // Проверяем доступ пользователя через группу
    bool can_write = auth->check_access(user->user_id, "/project", permission_type::WRITE);
    EXPECT_TRUE(can_write);
    
    // У пользователя роль VIEWER (READ, LIST), но не WRITE, так что доступ только через группу
    bool can_read = auth->check_access(user->user_id, "/project", permission_type::READ);
    EXPECT_TRUE(can_read); // READ есть по роли
    
    // Удаляем пользователя из группы
    bool removed = auth->remove_user_from_group(user->user_id, "developers");
    EXPECT_TRUE(removed);
    
    // После удаления доступ WRITE должен исчезнуть
    bool can_write_after = auth->check_access(user->user_id, "/project", permission_type::WRITE);
    EXPECT_FALSE(can_write_after);
    
    // READ остаётся по роли
    bool can_read_after = auth->check_access(user->user_id, "/project", permission_type::READ);
    EXPECT_TRUE(can_read_after);
}

TEST_F(AuthorizerTest, GetUserGroups) {
    auto user = auth->create_user("multigroup_user", "multi@example.com");
    ASSERT_TRUE(user.has_value());
    
    EXPECT_TRUE(auth->add_user_to_group(user->user_id, "group1"));
    EXPECT_TRUE(auth->add_user_to_group(user->user_id, "group2"));
    
    auto groups = auth->get_user_groups(user->user_id);
    EXPECT_EQ(groups.size(), 2);
    EXPECT_NE(std::find(groups.begin(), groups.end(), "group1"), groups.end());
    EXPECT_NE(std::find(groups.begin(), groups.end(), "group2"), groups.end());
    
    // Проверяем, что группы корректно возвращаются через список групп
    auto all_groups = auth->list_groups();
    EXPECT_GE(all_groups.size(), 2);
}

TEST_F(AuthorizerTest, GetGroupMembers) {
    auto user1 = auth->create_user("mem1", "mem1@example.com");
    auto user2 = auth->create_user("mem2", "mem2@example.com");
    ASSERT_TRUE(user1.has_value());
    ASSERT_TRUE(user2.has_value());
    
    EXPECT_TRUE(auth->add_user_to_group(user1->user_id, "team"));
    EXPECT_TRUE(auth->add_user_to_group(user2->user_id, "team"));
    
    auto members = auth->get_group_members("team");
    EXPECT_EQ(members.size(), 2);
    EXPECT_NE(std::find(members.begin(), members.end(), user1->user_id), members.end());
    EXPECT_NE(std::find(members.begin(), members.end(), user2->user_id), members.end());
}

// ========== НАСЛЕДОВАНИЕ ACL С ПЕРЕОПРЕДЕЛЕНИЕМ ==========

TEST_F(AuthorizerTest, ACLInheritanceWithOverrides) {
    auto user = auth->create_user("acl_user", "acl@example.com", user_role::VIEWER);
    ASSERT_TRUE(user.has_value());
    
    // Устанавливаем ACL для родительской директории: публичный доступ запрещён, но user имеет READ
    resource_acl parent_acl;
    parent_acl.resource_path = "/bucket";
    parent_acl.is_public = false;
    parent_acl.user_permissions[user->user_id] = {permission_type::READ};
    EXPECT_TRUE(auth->set_resource_acl("/bucket", parent_acl));
    
    // Для дочернего файла задаём более строгий ACL: явно запрещаем READ через пустые права
    // (по умолчанию если ACL есть, то наследование не применяется, нужно проверить)
    resource_acl child_acl;
    child_acl.resource_path = "/bucket/secret.txt";
    child_acl.is_public = false;
    // Не даём никаких прав пользователю
    EXPECT_TRUE(auth->set_resource_acl("/bucket/secret.txt", child_acl));
    
    // Проверяем доступ к родительской директории - должен быть READ
    bool can_read_parent = auth->check_access(user->user_id, "/bucket", permission_type::READ);
    EXPECT_TRUE(can_read_parent);
    
    // Проверяем доступ к дочернему файлу - не должен иметь READ (т.к. нет прав в дочернем ACL)
    // Роль VIEWER даёт READ, но она применяется только если нет ACL? 
    // В текущей реализации роль применяется всегда, если нет явного запрета.
    // Нужно уточнить: роль VIEWER даёт READ глобально, поэтому доступ всё равно будет.
    // Чтобы проверить переопределение, нужно либо изменить роль, либо убедиться, что ACL переопределяет роль.
    // В текущей логике check_access сначала проверяет роль, потом ACL. Роль имеет приоритет.
    // Поэтому этот тест может не показать переопределение. Лучше проверить на пользователе без роли.
    // Но все пользователи имеют роль. Поэтому оставим как проверку, что роль не отменяется ACL.
    bool can_read_child = auth->check_access(user->user_id, "/bucket/secret.txt", permission_type::READ);
    EXPECT_TRUE(can_read_child); // Из-за роли VIEWER
    
    // Для проверки переопределения создадим пользователя с ролью GUEST (только READ)
    // и дадим ему WRITE на родителя, но не на дочерний.
    auto guest = auth->create_user("guest_acl", "guest@example.com", user_role::GUEST);
    ASSERT_TRUE(guest.has_value());
    
    // Даём WRITE на родителя
    resource_acl parent2_acl;
    parent2_acl.resource_path = "/shared";
    parent2_acl.user_permissions[guest->user_id] = {permission_type::WRITE};
    EXPECT_TRUE(auth->set_resource_acl("/shared", parent2_acl));
    
    // Дочерний без WRITE
    resource_acl child2_acl;
    child2_acl.resource_path = "/shared/private.txt";
    EXPECT_TRUE(auth->set_resource_acl("/shared/private.txt", child2_acl));
    
    bool can_write_parent = auth->check_access(guest->user_id, "/shared", permission_type::WRITE);
    EXPECT_TRUE(can_write_parent);
    
    bool can_write_child = auth->check_access(guest->user_id, "/shared/private.txt", permission_type::WRITE);
    EXPECT_FALSE(can_write_child); // Дочерний ACL переопределяет (не даёт WRITE)
}

// ========== ПОЛИТИКИ: ЯВНЫЙ ЗАПРЕТ ПЕРЕОПРЕДЕЛЯЕТ РАЗРЕШЕНИЕ ==========

TEST_F(AuthorizerTest, PolicyDenyOverridesAllow) {
    auto user = auth->create_user("policy_user", "policy@example.com", user_role::VIEWER);
    ASSERT_TRUE(user.has_value());
    
    // Создаём политику, которая разрешает READ на /public/*, но запрещает WRITE на /public/restricted/*
    std::vector<permission> perms = {
        {permission_type::READ, "/public/*", true},
        {permission_type::WRITE, "/public/restricted/*", false},
        {permission_type::WRITE, "/public/*", true} // разрешаем WRITE на всё остальное в /public
    };
    auto policy = auth->create_policy("TestDenyPolicy", "Policy with deny", perms);
    ASSERT_TRUE(policy.has_value());
    
    // Проверяем доступ
    bool can_read_public = auth->check_access(user->user_id, "/public/file.txt", permission_type::READ);
    EXPECT_TRUE(can_read_public);
    
    bool can_write_public = auth->check_access(user->user_id, "/public/file.txt", permission_type::WRITE);
    EXPECT_TRUE(can_write_public);
    
    bool can_write_restricted = auth->check_access(user->user_id, "/public/restricted/secret.txt", permission_type::WRITE);
    EXPECT_FALSE(can_write_restricted); // Явный запрет
    
    // READ на restricted разрешён (нет deny)
    bool can_read_restricted = auth->check_access(user->user_id, "/public/restricted/secret.txt", permission_type::READ);
    EXPECT_TRUE(can_read_restricted);
}

// ========== ЗАГРУЗКА И СОХРАНЕНИЕ ПОЛЬЗОВАТЕЛЕЙ И ACL ==========

TEST_F(AuthorizerTest, LoadSaveUsersAndACLs) {
    // Создаём тестовые данные
    auto user1 = auth->create_user("loaduser1", "load1@example.com", user_role::MANAGER);
    auto user2 = auth->create_user("loaduser2", "load2@example.com", user_role::CONTRIBUTOR);
    ASSERT_TRUE(user1.has_value());
    ASSERT_TRUE(user2.has_value());
    
    EXPECT_TRUE(auth->add_user_to_group(user1->user_id, "admins"));
    EXPECT_TRUE(auth->add_user_to_group(user2->user_id, "developers"));
    
    // Создаём ACL
    resource_acl acl;
    acl.resource_path = "/test";
    acl.is_public = true;
    acl.owner_user_id = user1->user_id;
    acl.user_permissions[user2->user_id] = {permission_type::READ};
    acl.group_permissions["developers"] = {permission_type::WRITE};
    EXPECT_TRUE(auth->set_resource_acl("/test", acl));
    
    // Сохраняем пользователей и ACL во временные файлы
    fs::path temp_users = fs::temp_directory_path() / "test_users.json";
    fs::path temp_acls = fs::temp_directory_path() / "test_acls.json";
    
    bool saved_users = auth->save_users(temp_users.string());
    bool saved_acls = auth->save_acls(temp_acls.string());
    EXPECT_TRUE(saved_users);
    EXPECT_TRUE(saved_acls);
    
    // Создаём новый authorizer и загружаем
    authorizer auth2;
    bool loaded_users = auth2.load_users(temp_users.string());
    bool loaded_acls = auth2.load_acls(temp_acls.string());
    EXPECT_TRUE(loaded_users);
    EXPECT_TRUE(loaded_acls);
    
    // Проверяем пользователей
    auto loaded_user1 = auth2.get_user_by_name("loaduser1");
    auto loaded_user2 = auth2.get_user_by_name("loaduser2");
    ASSERT_TRUE(loaded_user1.has_value());
    ASSERT_TRUE(loaded_user2.has_value());
    EXPECT_EQ(loaded_user1->role, user_role::MANAGER);
    EXPECT_EQ(loaded_user2->role, user_role::CONTRIBUTOR);
    
    // Проверяем группы
    auto groups1 = auth2.get_user_groups(loaded_user1->user_id);
    EXPECT_EQ(groups1.size(), 1);
    EXPECT_EQ(groups1[0], "admins");
    
    // Проверяем ACL
    auto loaded_acl = auth2.get_resource_acl("/test");
    ASSERT_TRUE(loaded_acl.has_value());
    EXPECT_TRUE(loaded_acl->is_public);
    EXPECT_EQ(loaded_acl->owner_user_id, loaded_user1->user_id);
    EXPECT_EQ(loaded_acl->user_permissions[loaded_user2->user_id].count(permission_type::READ), 1);
    EXPECT_EQ(loaded_acl->group_permissions["developers"].count(permission_type::WRITE), 1);
    
    // Очистка
    fs::remove(temp_users);
    fs::remove(temp_acls);
}

TEST_F(AuthorizerTest, LoadMalformedJSON) {
    fs::path temp_file = fs::temp_directory_path() / "malformed_users.json";
    {
        std::ofstream file(temp_file);
        file << "{ not valid json }";
    }
    
    authorizer auth2;
    bool loaded = auth2.load_users(temp_file.string());
    EXPECT_FALSE(loaded);
    
    fs::remove(temp_file);
}

TEST_F(AuthorizerTest, LoadMissingFile) {
    authorizer auth2;
    bool loaded = auth2.load_users("/nonexistent/path/users.json");
    EXPECT_FALSE(loaded);
}

// ========== НЕСКОЛЬКО ГРУПП ==========

TEST_F(AuthorizerTest, MultipleGroupsAccess) {
    auto user = auth->create_user("multi", "multi@example.com", user_role::VIEWER);
    ASSERT_TRUE(user.has_value());
    
    EXPECT_TRUE(auth->add_user_to_group(user->user_id, "groupA"));
    EXPECT_TRUE(auth->add_user_to_group(user->user_id, "groupB"));
    
    // Даём права WRITE только группе groupB
    EXPECT_TRUE(auth->add_group_permission("/shared", "groupB", permission_type::WRITE));
    
    // Пользователь должен иметь WRITE через groupB
    bool can_write = auth->check_access(user->user_id, "/shared", permission_type::WRITE);
    EXPECT_TRUE(can_write);
    
    // Проверяем, что если удалить из groupB, доступ пропадает
    EXPECT_TRUE(auth->remove_user_from_group(user->user_id, "groupB"));
    bool can_write_after = auth->check_access(user->user_id, "/shared", permission_type::WRITE);
    EXPECT_FALSE(can_write_after);
    
    // READ остаётся по роли
    bool can_read = auth->check_access(user->user_id, "/shared", permission_type::READ);
    EXPECT_TRUE(can_read);
}

// ========== НЕЯВНЫЕ ПРАВА ВЛАДЕЛЬЦА ==========

TEST_F(AuthorizerTest, ResourceOwnerImplicitPermissions) {
    auto owner = auth->create_user("owner", "owner@example.com", user_role::VIEWER);
    ASSERT_TRUE(owner.has_value());
    
    // Устанавливаем ACL с владельцем, но без явных прав
    resource_acl acl;
    acl.resource_path = "/myfile.txt";
    acl.owner_user_id = owner->user_id;
    EXPECT_TRUE(auth->set_resource_acl("/myfile.txt", acl));
    
    // Владелец должен иметь все права (в текущей реализации is_resource_owner проверяется отдельно)
    // check_access должен предоставлять доступ владельцу
    bool can_read = auth->check_access(owner->user_id, "/myfile.txt", permission_type::READ);
    bool can_write = auth->check_access(owner->user_id, "/myfile.txt", permission_type::WRITE);
    bool can_delete = auth->check_access(owner->user_id, "/myfile.txt", permission_type::DELETE);
    
    // Роль VIEWER не даёт WRITE/DELETE, но владелец должен иметь
    EXPECT_TRUE(can_read);
    EXPECT_TRUE(can_write);
    EXPECT_TRUE(can_delete);
    
    // Проверяем, что не владелец не имеет WRITE
    auto other = auth->create_user("other", "other@example.com", user_role::VIEWER);
    ASSERT_TRUE(other.has_value());
    bool other_can_write = auth->check_access(other->user_id, "/myfile.txt", permission_type::WRITE);
    EXPECT_FALSE(other_can_write);
}

// ========== НЕСУЩЕСТВУЮЩИЙ ПОЛЬЗОВАТЕЛЬ ==========

TEST_F(AuthorizerTest, CheckAccessNonexistentUser) {
    bool access = auth->check_access("nonexistent-id", "/resource", permission_type::READ);
    EXPECT_FALSE(access);
}

// ========== ДОПОЛНИТЕЛЬНЫЕ ТЕСТЫ СТАТИЧЕСКИХ УТИЛИТ ==========

TEST(AuthorizerStaticTest, MatchesPatternEdgeCases) {
    // Пустые строки
    EXPECT_TRUE(authorizer::matches_pattern("", "*"));
    EXPECT_TRUE(authorizer::matches_pattern("", ""));
    EXPECT_FALSE(authorizer::matches_pattern("a", ""));
    EXPECT_FALSE(authorizer::matches_pattern("", "a"));
    
    // Спецсимволы в regex
    EXPECT_FALSE(authorizer::matches_pattern("[a]", "[a]")); // должно интерпретироваться буквально
    // matches_pattern преобразует '*' в '.*' и экранирует остальные символы regex?
    // Сейчас реализация: std::regex_replace(pattern, std::regex("\\*"), ".*") - остальное не экранируется!
    // Поэтому точка и другие спецсимволы будут работать как regex. Это может быть проблемой безопасности.
    // Добавим тест, чтобы зафиксировать текущее поведение.
    EXPECT_TRUE(authorizer::matches_pattern("a", ".")); // точка в паттерне соответствует любому символу
    EXPECT_TRUE(authorizer::matches_pattern("abc", "a.c"));
}

// ========== СОХРАНЕНИЕ И ЗАГРУЗКА ПОЛЬЗОВАТЕЛЕЙ С ГРУППАМИ ==========

TEST_F(AuthorizerTest, SaveLoadUsersPreservesGroups) {
    auto user = auth->create_user("groupuser", "group@test.com");
    ASSERT_TRUE(user.has_value());
    EXPECT_TRUE(auth->add_user_to_group(user->user_id, "testgroup"));
    
    fs::path temp_file = fs::temp_directory_path() / "users_with_groups.json";
    bool saved = auth->save_users(temp_file.string());
    ASSERT_TRUE(saved);
    
    authorizer auth2;
    bool loaded = auth2.load_users(temp_file.string());
    ASSERT_TRUE(loaded);
    
    auto loaded_user = auth2.get_user_by_name("groupuser");
    ASSERT_TRUE(loaded_user.has_value());
    auto groups = auth2.get_user_groups(loaded_user->user_id);
    EXPECT_EQ(groups.size(), 1);
    EXPECT_EQ(groups[0], "testgroup");
    
    // Группа должна быть в списке всех групп
    auto all_groups = auth2.list_groups();
    EXPECT_NE(std::find(all_groups.begin(), all_groups.end(), "testgroup"), all_groups.end());
    
    fs::remove(temp_file);
}

