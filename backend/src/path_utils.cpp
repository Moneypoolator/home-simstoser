#include "path_utils.hpp"
#include <glog/logging.h>

namespace path_utils {

bool is_path_safe(const fs::path& base_dir, const std::string& path) {
    return is_path_safe(base_dir, fs::path(path));
}

bool is_path_safe(const fs::path& base_dir, const fs::path& path) {
    if (path.empty()) {
        LOG(WARNING) << "Empty path provided";
        return false;
    }

    // Проверка на path traversal через ".."
    for (const auto& component : path) {
        if (component == ".." || component == "/" || component == ".") {
            LOG(WARNING) << "Path contains unsafe component: " << component.string();
            return false;
        }
    }

    // Абсолютные пути запрещены
    if (path.is_absolute()) {
        LOG(WARNING) << "Absolute path not allowed: " << path.string();
        return false;
    }

    try {
        // Нормализуем путь, разрешая символьные ссылки
        fs::path resolved = fs::weakly_canonical(base_dir / path);
        fs::path canonical_base = fs::weakly_canonical(base_dir);

        // Проверяем, что результирующий путь начинается с базовой директории
        auto resolved_str = resolved.string();
        auto base_str = canonical_base.string();
        
        if (resolved_str.size() < base_str.size() ||
            resolved_str.compare(0, base_str.size(), base_str) != 0) {
            LOG(WARNING) << "Path escapes base directory: " << path.string();
            return false;
        }

        // Дополнительная проверка: если resolved – симлинк, убедиться, что его цель также внутри base_dir
        if (fs::is_symlink(base_dir / path)) {
            fs::path target = fs::read_symlink(base_dir / path);
            // Если цель относительная, она уже учтена в weakly_canonical
            // Но можно дополнительно проверить абсолютную цель
            if (target.is_absolute()) {
                fs::path target_canon = fs::weakly_canonical(target);
                if (target_canon.string().compare(0, base_str.size(), base_str) != 0) {
                    LOG(WARNING) << "Symlink points outside base directory: " << path.string()
                                 << " -> " << target.string();
                    return false;
                }
            }
        }

        VLOG(2) << "Path is safe: " << path.string();
        return true;
    } catch (const std::exception& e) {
        LOG(ERROR) << "Exception during path safety check: " << e.what();
        return false;
    }
}

} // namespace path_utils