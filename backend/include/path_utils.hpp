#pragma once

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace path_utils {

// Проверяет, что путь `path` безопасен относительно базовой директории `base_dir`
// Возвращает true, если:
//   - путь не пуст
//   - не содержит ".." или "/" в качестве отдельных компонентов
//   - не является абсолютным
//   - канонический путь (с учётом символьных ссылок) лежит внутри base_dir
//   - не является символьной ссылкой, ведущей за пределы base_dir
[[nodiscard]] bool is_path_safe(const fs::path& base_dir, const std::string& path);

// Перегрузка для fs::path
[[nodiscard]] bool is_path_safe(const fs::path& base_dir, const fs::path& path);

} // namespace path_utils