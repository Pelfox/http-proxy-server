#pragma once

#include "cache_entry.hpp"

#include <list>
#include <optional>
#include <string>

class Cache
{
public:
    Cache(int maxSize, int defaultTTL);

    std::optional<CacheEntry> get(const std::string &url);
    void put(const std::string &url, const HttpResponse &response);
    void remove(const std::string &url);
    void clear();

private:
    std::list<CacheEntry> entries;
    int maxSize;
    int defaultTTL;
};
