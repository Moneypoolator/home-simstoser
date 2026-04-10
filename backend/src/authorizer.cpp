#include "authorizer.hpp"
#include <random>
#include <algorithm>
#include <regex>
#include <glog/logging.h>
#include "logging.hpp"


#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

authorizer::authorizer() {
    LOG(INFO) << "Authorizer initialized";
}

// ========== Управление пользователями ==========


// Вспомогательная функция для преобразования времени в строку ISO 8601
static std::string timepoint_to_iso8601(const std::chrono::system_clock::time_point& tp) {
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm* gmt = std::gmtime(&tt);
    std::stringstream ss;
    ss << std::put_time(gmt, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

// Вспомогательная функция для парсинга ISO 8601 в time_point
static std::optional<std::chrono::system_clock::time_point> iso8601_to_timepoint(const std::string& str) {
    std::tm tm = {};
    std::istringstream ss(str);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    if (ss.fail()) {
        return std::nullopt;
    }
    std::time_t time = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time);
}

bool authorizer::load_users(const std::string& filepath) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    std::ifstream file(filepath);
    if (!file) {
        LOG(WARNING) << "Users file not found: " << filepath;
        return false;
    }
    
    try {
        json data = json::parse(file);
        if (!data.is_array()) {
            LOG(ERROR) << "Users file must contain a JSON array";
            return false;
        }
        
        _users.clear();
        for (const auto& item : data) {
            user u;
            
            // user_id (опционально, генерируем если нет)
            if (item.contains("user_id") && item["user_id"].is_string()) {
                u.user_id = item["user_id"];
            } else {
                u.user_id = generate_id();
            }
            
            // username (обязателен)
            if (!item.contains("username") || !item["username"].is_string()) {
                LOG(WARNING) << "Skipping user entry: missing username";
                continue;
            }
            u.username = item["username"];
            
            // email (опционально)
            if (item.contains("email") && item["email"].is_string()) {
                u.email = item["email"];
            }
            
            // role (опционально, по умолчанию VIEWER)
            if (item.contains("role") && item["role"].is_string()) {
                auto role_opt = parse_role(item["role"]);
                u.role = role_opt.value_or(user_role::VIEWER);
            } else {
                u.role = user_role::VIEWER;
            }
            
            // is_active (опционально, по умолчанию true)
            u.is_active = true;
            if (item.contains("is_active") && item["is_active"].is_boolean()) {
                u.is_active = item["is_active"];
            }
            
            // created_at (опционально, иначе текущее время)
            u.created_at = std::chrono::system_clock::now();
            if (item.contains("created_at") && item["created_at"].is_string()) {
                auto tp = iso8601_to_timepoint(item["created_at"]);
                if (tp) u.created_at = *tp;
            }
            
            // last_login (опционально, иначе created_at)
            u.last_login = u.created_at;
            if (item.contains("last_login") && item["last_login"].is_string()) {
                auto tp = iso8601_to_timepoint(item["last_login"]);
                if (tp) u.last_login = *tp;
            }
            
            _users[u.user_id] = u;
            LOG(INFO) << "Loaded user: " << u.username 
                      << " (ID: " << u.user_id 
                      << ", role: " << role_to_string(u.role) << ")";
        }
        
        LOG(INFO) << "Loaded " << _users.size() << " users from " << filepath;
        return true;
    } catch (const json::exception& e) {
        LOG(ERROR) << "Failed to parse users file: " << e.what();
        return false;
    }
}

bool authorizer::save_users(const std::string& filepath) const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    json data = json::array();
    for (const auto& [id, u] : _users) {
        json item;
        item["user_id"] = u.user_id;
        item["username"] = u.username;
        item["email"] = u.email;
        item["role"] = role_to_string(u.role);
        item["is_active"] = u.is_active;
        item["created_at"] = timepoint_to_iso8601(u.created_at);
        item["last_login"] = timepoint_to_iso8601(u.last_login);
        data.push_back(item);
    }
    
    std::ofstream file(filepath);
    if (!file) {
        LOG(ERROR) << "Failed to open users file for writing: " << filepath;
        return false;
    }
    
    file << data.dump(4);
    LOG(INFO) << "Saved " << _users.size() << " users to " << filepath;
    return true;
}

std::optional<user> authorizer::create_user(
    const std::string& username,
    const std::string& email,
    user_role role)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    // Проверяем, не существует ли уже пользователь с таким именем
    for (const auto& [id, existing_user] : _users) {
        if (existing_user.username == username) {
            LOG(WARNING) << "User already exists: " << username;
            return std::nullopt;
        }
    }
    
    user new_user;
    new_user.user_id = generate_id();
    new_user.username = username;
    new_user.email = email;
    new_user.role = role;
    new_user.is_active = true;
    new_user.created_at = std::chrono::system_clock::now();
    new_user.last_login = new_user.created_at;
    
    _users[new_user.user_id] = new_user;
    
    LOG(INFO) << "Created user: " << username << " (ID: " << new_user.user_id 
              << ", role: " << role_to_string(role) << ")";
    
    return new_user;
}

std::optional<user> authorizer::get_user(const std::string& user_id) const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto it = _users.find(user_id);
    if (it == _users.end()) {
        return std::nullopt;
    }
    
    return it->second;
}

std::optional<user> authorizer::get_user_by_name(const std::string& username) const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    for (const auto& [id, user] : _users) {
        if (user.username == username) {
            return user;
        }
    }
    
    return std::nullopt;
}

bool authorizer::update_user_role(const std::string& user_id, user_role new_role) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto it = _users.find(user_id);
    if (it == _users.end()) {
        LOG(WARNING) << "User not found: " << user_id;
        return false;
    }
    
    auto old_role = it->second.role;
    it->second.role = new_role;
    
    LOG(INFO) << "Updated user role: " << it->second.username 
              << " (" << role_to_string(old_role) << " -> " 
              << role_to_string(new_role) << ")";
    
    return true;
}

bool authorizer::activate_user(const std::string& user_id) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto it = _users.find(user_id);
    if (it == _users.end()) {
        return false;
    }
    
    it->second.is_active = true;
    LOG(INFO) << "Activated user: " << it->second.username;
    return true;
}

bool authorizer::deactivate_user(const std::string& user_id) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto it = _users.find(user_id);
    if (it == _users.end()) {
        return false;
    }
    
    it->second.is_active = false;
    LOG(INFO) << "Deactivated user: " << it->second.username;
    return true;
}

bool authorizer::delete_user(const std::string& user_id) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto it = _users.find(user_id);
    if (it == _users.end()) {
        return false;
    }
    
    std::string username = it->second.username;
    _users.erase(it);
    
    LOG(INFO) << "Deleted user: " << username;
    return true;
}

std::vector<user> authorizer::list_users() const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    std::vector<user> users;
    users.reserve(_users.size());
    
    for (const auto& [id, user] : _users) {
        users.push_back(user);
    }
    
    return users;
}

// ========== Управление политиками доступа ==========

std::optional<access_policy> authorizer::create_policy(
    const std::string& name,
    const std::string& description,
    const std::vector<permission>& permissions)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    access_policy policy;
    policy.policy_id = generate_id();
    policy.name = name;
    policy.description = description;
    policy.permissions = permissions;
    policy.created_at = std::chrono::system_clock::now();
    
    _policies[policy.policy_id] = policy;
    
    LOG(INFO) << "Created policy: " << name << " (ID: " << policy.policy_id << ")";
    
    return policy;
}

std::optional<access_policy> authorizer::get_policy(const std::string& policy_id) const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto it = _policies.find(policy_id);
    if (it == _policies.end()) {
        return std::nullopt;
    }
    
    return it->second;
}

bool authorizer::delete_policy(const std::string& policy_id) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto it = _policies.find(policy_id);
    if (it == _policies.end()) {
        return false;
    }
    
    _policies.erase(it);
    LOG(INFO) << "Deleted policy: " << policy_id;
    return true;
}

std::vector<access_policy> authorizer::list_policies() const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    std::vector<access_policy> policies;
    policies.reserve(_policies.size());
    
    for (const auto& [id, policy] : _policies) {
        policies.push_back(policy);
    }
    
    return policies;
}

// ========== Управление ACL ресурсов ==========

bool authorizer::set_resource_acl(
    const std::string& resource_path,
    const resource_acl& acl)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    _resource_acls[resource_path] = acl;
    
    LOG(INFO) << "Set ACL for resource: " << resource_path;
    return true;
}

std::optional<resource_acl> authorizer::get_resource_acl(const std::string& resource_path) const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    // Сначала ищем точное совпадение
    auto it = _resource_acls.find(resource_path);
    if (it != _resource_acls.end()) {
        return it->second;
    }
    
    // Затем ищем ACL для родительских директорий
    fs::path current(resource_path);
    VLOG(2) << "Starting parent search for path: " << resource_path;
    while (true) {
        fs::path parent = current.parent_path();
        if (parent.empty() || parent == current) {
            VLOG(2) << "Reached root or same path, stopping";
            break;
        }
        current = parent;
        std::string current_str = current.string();
        VLOG(2) << "Checking parent path: " << current_str;
        auto parent_it = _resource_acls.find(current_str);
        if (parent_it != _resource_acls.end()) {
            VLOG(2) << "Found ACL for parent: " << current_str;
            return parent_it->second;
        }
    }
    VLOG(2) << "No parent ACL found";
    
    return std::nullopt;
}

bool authorizer::remove_resource_acl(const std::string& resource_path) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto it = _resource_acls.find(resource_path);
    if (it == _resource_acls.end()) {
        return false;
    }
    
    _resource_acls.erase(it);
    LOG(INFO) << "Removed ACL for resource: " << resource_path;
    return true;
}

bool authorizer::make_resource_public(const std::string& resource_path) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto acl_it = _resource_acls.find(resource_path);
    if (acl_it == _resource_acls.end()) {
        resource_acl new_acl;
        new_acl.resource_path = resource_path;
        new_acl.is_public = true;
        _resource_acls[resource_path] = new_acl;
    } else {
        acl_it->second.is_public = true;
    }
    
    LOG(INFO) << "Made resource public: " << resource_path;
    return true;
}

bool authorizer::make_resource_private(const std::string& resource_path) {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto acl_it = _resource_acls.find(resource_path);
    if (acl_it == _resource_acls.end()) {
        resource_acl new_acl;
        new_acl.resource_path = resource_path;
        new_acl.is_public = false;
        _resource_acls[resource_path] = new_acl;
    } else {
        acl_it->second.is_public = false;
    }
    
    LOG(INFO) << "Made resource private: " << resource_path;
    return true;
}

bool authorizer::add_user_permission(
    const std::string& resource_path,
    const std::string& user_id,
    permission_type perm)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto acl_it = _resource_acls.find(resource_path);
    if (acl_it == _resource_acls.end()) {
        resource_acl new_acl;
        new_acl.resource_path = resource_path;
        new_acl.user_permissions[user_id].insert(perm);
        _resource_acls[resource_path] = new_acl;
    } else {
        acl_it->second.user_permissions[user_id].insert(perm);
    }
    
    LOG(INFO) << "Added permission for user " << user_id 
              << " on resource " << resource_path 
              << ": " << permission_to_string(perm);
    
    return true;
}

bool authorizer::remove_user_permission(
    const std::string& resource_path,
    const std::string& user_id,
    permission_type perm)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto acl_it = _resource_acls.find(resource_path);
    if (acl_it == _resource_acls.end()) {
        return false;
    }
    
    auto user_it = acl_it->second.user_permissions.find(user_id);
    if (user_it == acl_it->second.user_permissions.end()) {
        return false;
    }
    
    size_t erased = user_it->second.erase(perm);
    if (erased == 0) {
        return false;
    }
    
    if (user_it->second.empty()) {
        acl_it->second.user_permissions.erase(user_it);
    }
    
    LOG(INFO) << "Removed permission for user " << user_id
              << " on resource " << resource_path
              << ": " << permission_to_string(perm);
    
    return true;
}

bool authorizer::add_group_permission(
    const std::string& resource_path,
    const std::string& group_name,
    permission_type perm)
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto acl_it = _resource_acls.find(resource_path);
    if (acl_it == _resource_acls.end()) {
        resource_acl new_acl;
        new_acl.resource_path = resource_path;
        new_acl.group_permissions[group_name].insert(perm);
        _resource_acls[resource_path] = new_acl;
    } else {
        acl_it->second.group_permissions[group_name].insert(perm);
    }
    
    LOG(INFO) << "Added permission for group " << group_name 
              << " on resource " << resource_path 
              << ": " << permission_to_string(perm);
    
    return true;
}

// ========== Проверка доступа ==========

bool authorizer::check_access(
    const std::string& user_id,
    const std::string& resource_path,
    permission_type required_permission) const
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    // Получаем пользователя
    auto user_it = _users.find(user_id);
    if (user_it == _users.end() || !user_it->second.is_active) {
        LOG(WARNING) << "Access denied: user not found or inactive - " << user_id;
        return false;
    }
    
    const user& current_user = user_it->second;
    
    // Администраторы имеют полный доступ
    if (current_user.role == user_role::ADMIN) {
        VLOG(2) << "Access granted to admin: " << current_user.username;
        return true;
    }
    
    // Проверяем права на основе роли
    auto role_permissions = get_permissions_for_role(current_user.role);
    if (role_permissions.count(required_permission) > 0) {
        VLOG(2) << "Access granted by role: " << current_user.username 
                << " (" << role_to_string(current_user.role) << ")";
        return true;
    }
    
    // Проверяем ACL ресурса
    auto acl_opt = get_resource_acl(resource_path);
    if (acl_opt) {
        const resource_acl& acl = *acl_opt;
        
        // Проверяем, является ли пользователь владельцем
        if (acl.owner_user_id && *acl.owner_user_id == user_id) {
            VLOG(2) << "Access granted to resource owner: " << current_user.username;
            return true;
        }
        
        // Проверяем права пользователя в ACL
        auto user_perms_it = acl.user_permissions.find(user_id);
        if (user_perms_it != acl.user_permissions.end()) {
            if (user_perms_it->second.count(required_permission) > 0) {
                VLOG(2) << "Access granted by ACL for user: " << current_user.username;
                return true;
            }
        }
        
        // TODO: Проверка групповых прав
        
        // Если есть явный запрет
        // (реализация явных запретов может быть добавлена позже)
    }
    
    // Проверяем политики
    if (check_policy_permissions(user_id, resource_path, required_permission)) {
        VLOG(2) << "Access granted by policy";
        return true;
    }
    
    LOG(WARNING) << "Access denied for user " << current_user.username 
                 << " to resource " << resource_path 
                 << " (permission: " << permission_to_string(required_permission) << ")";
    
    return false;
}

bool authorizer::check_public_access(
    const std::string& resource_path,
    permission_type required_permission) const
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    // Проверяем, есть ли публичный доступ к ресурсу
    auto acl_opt = get_resource_acl(resource_path);
    if (!acl_opt || !acl_opt->is_public) {
        return false;
    }
    
    // Для публичного доступа разрешаем только чтение и просмотр списка
    if (required_permission == permission_type::READ || 
        required_permission == permission_type::LIST) {
        VLOG(2) << "Public access granted to resource: " << resource_path;
        return true;
    }
    
    return false;
}

bool authorizer::is_admin(const std::string& user_id) const {
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto it = _users.find(user_id);
    if (it == _users.end()) {
        return false;
    }
    
    return it->second.role == user_role::ADMIN;
}

bool authorizer::is_resource_owner(
    const std::string& user_id,
    const std::string& resource_path) const
{
    std::lock_guard<std::recursive_mutex> lock(_mutex);
    
    auto acl_opt = get_resource_acl(resource_path);
    if (!acl_opt) {
        return false;
    }
    
    return acl_opt->owner_user_id && *acl_opt->owner_user_id == user_id;
}

// ========== Вспомогательные методы ==========

std::optional<permission_type> authorizer::parse_permission(const std::string& str) {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    
    if (lower_str == "read") return permission_type::READ;
    if (lower_str == "write") return permission_type::WRITE;
    if (lower_str == "delete") return permission_type::DELETE;
    if (lower_str == "list") return permission_type::LIST;
    if (lower_str == "manage_acl") return permission_type::MANAGE_ACL;
    
    return std::nullopt;
}

std::string authorizer::permission_to_string(permission_type perm) {
    switch (perm) {
        case permission_type::READ: return "READ";
        case permission_type::WRITE: return "WRITE";
        case permission_type::DELETE: return "DELETE";
        case permission_type::LIST: return "LIST";
        case permission_type::MANAGE_ACL: return "MANAGE_ACL";
        default: return "UNKNOWN";
    }
}

std::optional<user_role> authorizer::parse_role(const std::string& str) {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    
    if (lower_str == "admin") return user_role::ADMIN;
    if (lower_str == "manager") return user_role::MANAGER;
    if (lower_str == "contributor") return user_role::CONTRIBUTOR;
    if (lower_str == "viewer") return user_role::VIEWER;
    if (lower_str == "guest") return user_role::GUEST;
    
    return std::nullopt;
}

std::string authorizer::role_to_string(user_role role) {
    switch (role) {
        case user_role::ADMIN: return "ADMIN";
        case user_role::MANAGER: return "MANAGER";
        case user_role::CONTRIBUTOR: return "CONTRIBUTOR";
        case user_role::VIEWER: return "VIEWER";
        case user_role::GUEST: return "GUEST";
        default: return "UNKNOWN";
    }
}

bool authorizer::matches_pattern(const std::string& path, const std::string& pattern) {
    // Преобразуем паттерн в регулярное выражение
    // '*' в паттерне означает любое количество символов
    std::string regex_pattern = std::regex_replace(pattern, std::regex("\\*"), ".*");
    regex_pattern = "^" + regex_pattern + "$";
    
    try {
        std::regex re(regex_pattern);
        return std::regex_match(path, re);
    } catch (const std::regex_error&) {
        return false;
    }
}

std::string authorizer::generate_id() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    ss << std::hex;
    
    // Генерируем 16-символьный hex ID
    for (int i = 0; i < 16; ++i) {
        ss << dis(gen);
    }
    
    return ss.str();
}

std::set<permission_type> authorizer::get_permissions_for_role(user_role role) const {
    std::set<permission_type> permissions;
    
    switch (role) {
        case user_role::ADMIN:
            permissions = {
                permission_type::READ,
                permission_type::WRITE,
                permission_type::DELETE,
                permission_type::LIST,
                permission_type::MANAGE_ACL
            };
            break;
            
        case user_role::MANAGER:
            permissions = {
                permission_type::READ,
                permission_type::WRITE,
                permission_type::DELETE,
                permission_type::LIST
            };
            break;
            
        case user_role::CONTRIBUTOR:
            permissions = {
                permission_type::READ,
                permission_type::WRITE,
                permission_type::LIST
            };
            break;
            
        case user_role::VIEWER:
            permissions = {
                permission_type::READ,
                permission_type::LIST
            };
            break;
            
        case user_role::GUEST:
            permissions = {
                permission_type::READ
            };
            break;
            
        default:
            break;
    }
    
    return permissions;
}

bool authorizer::check_policy_permissions(
    const std::string& user_id,
    const std::string& resource_path,
    permission_type required_permission) const
{
    // TODO: Реализация проверки политик
    // Пока возвращаем false, чтобы не давать доступ по умолчанию
    return false;
}