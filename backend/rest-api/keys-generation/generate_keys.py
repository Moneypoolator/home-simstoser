#!/usr/bin/env python3
"""
Генерация ключей доступа для S3 сервера
"""
import secrets
import string
import json
from datetime import datetime

def generate_access_key_id():
    """Генерирует публичный ключ (16-20 символов, начинается с AKIA)"""
    # AKIA + 16 случайных символов (буквы + цифры)
    chars = string.ascii_uppercase + string.digits
    return "AKIA" + ''.join(secrets.choice(chars) for _ in range(16))

def generate_secret_access_key():
    """Генерирует секретный ключ (40 символов)"""
    chars = string.ascii_letters + string.digits
    return ''.join(secrets.choice(chars) for _ in range(40))

def create_key_pair(username, permissions=None):
    """Создаёт пару ключей для пользователя"""
    return {
        "access_key_id": generate_access_key_id(),
        "secret_access_key": generate_secret_access_key(),
        "user_name": username,
        "active": True,
        "created_at": datetime.utcnow().isoformat() + "Z",
        "permissions": permissions or ["read", "write"]
    }

# Создаём ключи для пользователей
keys = {
    "user1": create_key_pair("user1", ["read", "write"]),
    "admin": create_key_pair("admin", ["read", "write", "delete", "list", "manage_acl"])
}

# Сохраняем в файл
with open('keys.json', 'w') as f:
    json.dump(keys, f, indent=2)

print("Ключи сгенерированы и сохранены в keys.json")
print("\nПример ключа:")
print(json.dumps(keys["user1"], indent=2))