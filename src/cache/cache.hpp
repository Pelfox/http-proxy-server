#pragma once

#include "cache_entry.hpp"

#include <chrono>
#include <cstddef>
#include <list>
#include <mutex>
#include <optional>
#include <string>

/**
 * @brief Потокобезопасный in-memory кэш HTTP-ответов с TTL и LRU-вытеснением.
 *
 * Записи держатся в порядке от свежей (front) к давней (back). Успешный get
 * перемещает запись в начало; put удаляет хвост, если размер превысил лимит.
 */
class Cache
{
public:
    Cache(std::size_t maxSize, std::chrono::seconds defaultTTL);

    std::optional<CacheEntry> get(const std::string &url);
    void put(const std::string &url, const HttpResponse &response);
    void remove(const std::string &url);
    void clear();

private:
    bool isExpired(const CacheEntry &entry) const;

    std::list<CacheEntry> entries;
    std::size_t maxSize;
    std::chrono::seconds defaultTTL;
    std::mutex mutex;
};
