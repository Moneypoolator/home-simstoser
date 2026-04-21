#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <optional>
#include <memory>
#include <mutex>
#include <chrono>
#include <filesystem>

namespace fs = std::filesystem;

// Типы разрешений
enum class permission_type {
    READ,           // Чтение файлов
    WRITE,          // Запись/загрузка файлов
    DELETE,         // Удаление файлов
    LIST,           // Просмотр списка файлов
    MANAGE_ACL      // Управление правами доступа
};

// Роли пользователей
enum class user_role {
    ADMIN,          // Полный доступ ко всему
    MANAGER,        // Управление файлами, но не настройками
    CONTRIBUTOR,    // Загрузка и чтение своих файлов
    VIEWER,         // Только чтение
    GUEST           // Ограниченный доступ
};

// Структура пользователя
struct user {
    std::string user_id;
    std::string username;
    std::string email;
    user_role role;
    bool is_active = true;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_login;
    std::vector<std::string> groups; // Группы, в которых состоит пользователь
};

// Структура разрешения
struct permission {
    permission_type type;
    std::string resource_pattern;  // Паттерн пути (например: "bucket/*" или "*/private/*")
    bool allow;                     // Разрешено или запрещено
};

// Структура политики доступа
struct access_policy {
    std::string policy_id;
    std::string name;
    std::string description;
    std::vector<permission> permissions;
    std::chrono::system_clock::time_point created_at;
};

// ACL (Access Control List) для конкретного ресурса
struct resource_acl {
    std::string resource_path;     // Путь к файлу или директории
    std::map<std::string, std::set<permission_type>> user_permissions;  // user_id -> permissions
    std::map<std::string, std::set<permission_type>> group_permissions; // group_name -> permissions
    bool is_public = false;        // Публичный доступ (без аутентификации)
    std::optional<std::string> owner_user_id;  // Владелец ресурса
};

class authorizer {
public:
    authorizer();
    ~authorizer() = default;
    
    // === Управление пользователями ===
    
    // Создать пользователя
    std::optional<user> create_user(
        const std::string& username,
        const std::string& email,
        user_role role = user_role::VIEWER
    );
    
    // Получить пользователя по ID или имени
    std::optional<user> get_user(const std::string& user_id) const;
    std::optional<user> get_user_by_name(const std::string& username) const;
    
    // Обновить роль пользователя
    [[nodiscard]] bool update_user_role(const std::string& user_id, user_role new_role);
    
    // Активировать/деактивировать пользователя
    [[nodiscard]] bool activate_user(const std::string& user_id);
    [[nodiscard]] bool deactivate_user(const std::string& user_id);
    
    // Удалить пользователя
    [[nodiscard]] bool delete_user(const std::string& user_id);
    
    // Список всех пользователей
    std::vector<user> list_users() const;
    
    // === Управление группами ===
    
    // Добавить пользователя в группу
    [[nodiscard]] bool add_user_to_group(const std::string& user_id, const std::string& group_name);
    
    // Удалить пользователя из группы
    [[nodiscard]] bool remove_user_from_group(const std::string& user_id, const std::string& group_name);
    
    // Получить список групп пользователя
    std::vector<std::string> get_user_groups(const std::string& user_id) const;
    
    // Получить список всех групп
    std::vector<std::string> list_groups() const;
    
    // Получить список участников группы
    std::vector<std::string> get_group_members(const std::string& group_name) const;
    
    // === Управление политиками доступа ===
    
    // Создать политику доступа
    std::optional<access_policy> create_policy(
        const std::string& name,
        const std::string& description,
        const std::vector<permission>& permissions
    );
    
    // Получить политику по ID
    std::optional<access_policy> get_policy(const std::string& policy_id) const;
    
    // Удалить политику
    [[nodiscard]] bool delete_policy(const std::string& policy_id);
    
    // Список всех политик
    std::vector<access_policy> list_policies() const;
    
    // === Управление правами доступа к ресурсам (ACL) ===
    
    // Установить права доступа к ресурсу
    [[nodiscard]] bool set_resource_acl(
        const std::string& resource_path,
        const resource_acl& acl
    );
    
    // Получить права доступа к ресурсу
    std::optional<resource_acl> get_resource_acl(const std::string& resource_path) const;
    
    // Удалить права доступа к ресурсу
    [[nodiscard]] bool remove_resource_acl(const std::string& resource_path);
    
    // Сделать ресурс публичным
    [[nodiscard]] bool make_resource_public(const std::string& resource_path);
    
    // Сделать ресурс приватным
    [[nodiscard]] bool make_resource_private(const std::string& resource_path);
    
    // Добавить право доступа для пользователя
    [[nodiscard]] bool add_user_permission(
        const std::string& resource_path,
        const std::string& user_id,
        permission_type perm
    );
    
    // Удалить право доступа для пользователя
    [[nodiscard]] bool remove_user_permission(
        const std::string& resource_path,
        const std::string& user_id,
        permission_type perm
    );
    
    // Добавить право доступа для группы
    [[nodiscard]] bool add_group_permission(
        const std::string& resource_path,
        const std::string& group_name,
        permission_type perm
    );
    
    // Удалить право доступа для группы
    [[nodiscard]] bool remove_group_permission(
        const std::string& resource_path,
        const std::string& group_name,
        permission_type perm
    );
    
    // === Проверка доступа ===
    
    // Проверить, имеет ли пользователь доступ к ресурсу
    [[nodiscard]] bool check_access(
        const std::string& user_id,
        const std::string& resource_path,
        permission_type required_permission
    ) const;
    
    // Проверить доступ для анонимного пользователя (публичный доступ)
    [[nodiscard]] bool check_public_access(
        const std::string& resource_path,
        permission_type required_permission
    ) const;
    
    // Проверить, является ли пользователь администратором
    [[nodiscard]] bool is_admin(const std::string& user_id) const;
    
    // Проверить, является ли пользователь владельцем ресурса
    [[nodiscard]] bool is_resource_owner(
        const std::string& user_id,
        const std::string& resource_path
    ) const;
    
    // === Утилиты ===
    
    // Преобразовать строку в тип разрешения
    static std::optional<permission_type> parse_permission(const std::string& str);
    
    // Преобразовать тип разрешения в строку
    static std::string permission_to_string(permission_type perm);
    
    // Преобразовать строку в роль
    static std::optional<user_role> parse_role(const std::string& str);
    
    // Преобразовать роль в строку
    static std::string role_to_string(user_role role);
    
    // Проверить соответствие паттерна пути
    [[nodiscard]] static bool matches_pattern(const std::string& path, const std::string& pattern);

    // Загрузить пользователей из JSON-файла
    [[nodiscard]] bool load_users(const std::string& filepath);
    
    // Сохранить пользователей в JSON-файл
    [[nodiscard]] bool save_users(const std::string& filepath) const;
    
    // Загрузить ACL из JSON-файла
    [[nodiscard]] bool load_acls(const std::string& filepath);
    
    // Сохранить ACL в JSON-файл
    [[nodiscard]] bool save_acls(const std::string& filepath) const;
    
private:
    mutable std::recursive_mutex _mutex;
    std::map<std::string, user> _users;
    std::map<std::string, access_policy> _policies;
    std::map<std::string, resource_acl> _resource_acls;
    std::map<std::string, std::set<std::string>> _group_members; // group_name -> set of user_ids
    
    // Генерация уникального ID
    std::string generate_id() const;
    
    // Получение прав пользователя на основе роли
    std::set<permission_type> get_permissions_for_role(user_role role) const;
    
    // Проверка прав на основе политик
    [[nodiscard]] bool check_policy_permissions(
        const std::string& user_id,
        const std::string& resource_path,
        permission_type required_permission
    ) const;
};