#pragma once

#include <string>
#include <vector>
#include <optional>
#include <unordered_map>
#include <list>
#include <mutex>
#include <chrono>
#include <memory>
#include <cstddef>

namespace fs = std::filesystem;

/**
 * @brief LRU cache with optional TTL (time-to-live) support.
 * 
 * Thread-safe cache that evicts least recently used items when capacity is exceeded.
 * Each entry can have an optional expiration time.
 * 
 * @tparam KeyType Key type (default std::string)
 * @tparam ValueType Value type
 */
template<typename KeyType = std::string, typename ValueType = std::vector<char>>
class lru_cache {
public:
    struct cache_entry {
        ValueType value;
        std::chrono::steady_clock::time_point timestamp;
        std::chrono::seconds ttl; // zero means no TTL
        std::size_t size; // size in bytes (for memory tracking)
    };

    /**
     * @brief Construct a new LRU cache.
     * 
     * @param max_size_bytes Maximum memory usage in bytes (0 = unlimited)
     * @param default_ttl Default TTL for entries (0 = no expiration)
     */
    explicit lru_cache(std::size_t max_size_bytes = 0, 
                       std::chrono::seconds default_ttl = std::chrono::seconds(0))
        : _max_size(max_size_bytes)
        , _current_size(0)
        , _default_ttl(default_ttl)
        , _hits(0)
        , _misses(0)
    {}

    /**
     * @brief Insert or update a key with given value.
     *
     * @param key
     * @param value
     * @param ttl Optional TTL (if not provided, uses default)
     * @return true if inserted, false if error (e.g., value size exceeds max size)
     */
    [[nodiscard]] bool put(const KeyType& key, const ValueType& value,
             std::chrono::seconds ttl = std::chrono::seconds(0)) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        std::size_t entry_size = compute_size(value);
        if (_max_size > 0 && entry_size > _max_size) {
            // Single entry too large to fit
            return false;
        }

        // If key already exists, remove old entry
        auto it = _map.find(key);
        if (it != _map.end()) {
            _current_size -= it->second.entry.size;
            _lru_list.erase(it->second.lru_it);
        }

        // Ensure capacity
        while (_max_size > 0 && _current_size + entry_size > _max_size && !_map.empty()) {
            evict_one();
        }

        // Insert new entry
        auto now = std::chrono::steady_clock::now();
        if (ttl.count() == 0) {
            ttl = _default_ttl;
        }
        _lru_list.push_front(key);
        cache_entry entry{
            value,
            now,
            ttl,
            entry_size
        };
        _map[key] = {entry, _lru_list.begin()};
        _current_size += entry_size;
        return true;
    }

    /**
     * @brief Retrieve value for key if present and not expired.
     * 
     * @param key 
     * @return std::optional<ValueType> 
     */
    std::optional<ValueType> get(const KeyType& key) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        auto it = _map.find(key);
        if (it == _map.end()) {
            _misses++;
            return std::nullopt;
        }

        // Check expiration
        auto& entry = it->second.entry;
        if (entry.ttl.count() > 0) {
            auto now = std::chrono::steady_clock::now();
            if (now - entry.timestamp > entry.ttl) {
                // Expired, remove
                _current_size -= entry.size;
                _lru_list.erase(it->second.lru_it);
                _map.erase(it);
                _misses++;
                return std::nullopt;
            }
        }

        // Update LRU order
        _lru_list.splice(_lru_list.begin(), _lru_list, it->second.lru_it);
        _hits++;
        return entry.value;
    }

    /**
     * @brief Check if key exists (and not expired).
     *
     * @param key
     * @return true
     * @return false
     */
    [[nodiscard]] bool contains(const KeyType& key) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _map.find(key);
        if (it == _map.end()) return false;
        
        if (it->second.entry.ttl.count() > 0) {
            auto now = std::chrono::steady_clock::now();
            if (now - it->second.entry.timestamp > it->second.entry.ttl) {
                // Expired, remove
                _current_size -= it->second.entry.size;
                _lru_list.erase(it->second.lru_it);
                _map.erase(it);
                return false;
            }
        }
        return true;
    }

    /**
     * @brief Remove entry for key.
     *
     * @param key
     * @return true if removed, false if not found
     */
    [[nodiscard]] bool erase(const KeyType& key) {
        std::lock_guard<std::mutex> lock(_mutex);
        auto it = _map.find(key);
        if (it == _map.end()) return false;
        
        _current_size -= it->second.entry.size;
        _lru_list.erase(it->second.lru_it);
        _map.erase(it);
        return true;
    }

    /**
     * @brief Clear all entries.
     */
    void clear() {
        std::lock_guard<std::mutex> lock(_mutex);
        _map.clear();
        _lru_list.clear();
        _current_size = 0;
        _hits = 0;
        _misses = 0;
    }

    /**
     * @brief Remove expired entries.
     * 
     * @return number of entries removed
     */
    std::size_t cleanup_expired() {
        std::lock_guard<std::mutex> lock(_mutex);
        std::size_t removed = 0;
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = _map.begin(); it != _map.end(); ) {
            if (it->second.entry.ttl.count() > 0 && 
                now - it->second.entry.timestamp > it->second.entry.ttl) {
                _current_size -= it->second.entry.size;
                _lru_list.erase(it->second.lru_it);
                it = _map.erase(it);
                removed++;
            } else {
                ++it;
            }
        }
        return removed;
    }

    /**
     * @brief Get current cache size in bytes.
     */
    std::size_t size_bytes() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _current_size;
    }

    /**
     * @brief Get number of entries.
     */
    std::size_t entry_count() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _map.size();
    }

    /**
     * @brief Get cache statistics.
     */
    struct stats {
        std::size_t hits;
        std::size_t misses;
        std::size_t entry_count;
        std::size_t size_bytes;
        std::size_t max_size_bytes;
    };

    stats get_stats() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return stats{_hits, _misses, _map.size(), _current_size, _max_size};
    }

    /**
     * @brief Set maximum cache size in bytes.
     */
    void set_max_size(std::size_t max_size_bytes) {
        std::lock_guard<std::mutex> lock(_mutex);
        _max_size = max_size_bytes;
        while (_max_size > 0 && _current_size > _max_size && !_map.empty()) {
            evict_one();
        }
    }

private:
    struct map_value {
        cache_entry entry;
        typename std::list<KeyType>::iterator lru_it;
    };

    std::unordered_map<KeyType, map_value> _map;
    std::list<KeyType> _lru_list; // most recent at front
    mutable std::mutex _mutex;
    std::size_t _max_size;
    std::size_t _current_size;
    std::chrono::seconds _default_ttl;
    std::size_t _hits;
    std::size_t _misses;

    void evict_one() {
        if (_lru_list.empty()) return;
        KeyType key = _lru_list.back();
        auto it = _map.find(key);
        if (it != _map.end()) {
            _current_size -= it->second.entry.size;
            _lru_list.pop_back();
            _map.erase(it);
        }
    }

    // Compute size of a value in bytes (specialize for different types)
    std::size_t compute_size(const ValueType& value) const {
        // Generic implementation: use sizeof for primitive types, but for vector<char> we want capacity
        if constexpr (std::is_same_v<ValueType, std::vector<char>>) {
            return value.capacity() * sizeof(char);
        } else if constexpr (std::is_same_v<ValueType, std::string>) {
            return value.capacity() * sizeof(char);
        } else {
            return sizeof(value);
        }
    }
};

// Specialization for file_metadata (defined in file_manager.hpp)
// We'll forward declare file_metadata and provide a compute_size specialization
// Since file_metadata includes strings and maps, we need to estimate size.
// We'll implement in cache.cpp

// Convenience aliases for our use cases
using file_content_cache = lru_cache<std::string, std::vector<char>>;
using metadata_cache = lru_cache<std::string, struct file_metadata>;