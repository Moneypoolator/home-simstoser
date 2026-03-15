#include "authenticator.hpp"

int main() {
    authenticator auth;
    
    // Создаем ключ для пользователя
    auto key = auth.create_access_key("myuser");
    
    if (key) {
        std::cout << "Access Key ID: " << key->access_key_id << std::endl;
        std::cout << "Secret Key: " << key->secret_access_key << std::endl;
    }
    
    // Сохраняем ключи в файл
    auth.save_keys("access_keys.csv");
    
    return 0;
}