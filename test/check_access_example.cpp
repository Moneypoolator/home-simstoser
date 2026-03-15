#include "authorizer.hpp"
#include <iostream>

int main() {
    authorizer auth;
    
    // Создаем пользователей
    auto admin = auth.create_user("admin", "admin@example.com", user_role::ADMIN);
    auto manager = auth.create_user("manager", "manager@example.com", user_role::MANAGER);
    auto viewer = auth.create_user("viewer", "viewer@example.com", user_role::VIEWER);
    
    // Делаем файл публичным
    auth.make_resource_public("public/document.pdf");
    
    // Добавляем права для конкретного пользователя
    auth.add_user_permission("private/report.pdf", viewer->user_id, permission_type::READ);
    
    // Проверяем доступ
    bool can_read = auth.check_access(viewer->user_id, "private/report.pdf", permission_type::READ);
    std::cout << "Viewer can read report: " << (can_read ? "YES" : "NO") << std::endl;
    
    return 0;
}