#include "cache.hpp"

Cache::Cache(int maxSize, int defaultTTL)
    : maxSize(maxSize), defaultTTL(defaultTTL) {}

std::optional<CacheEntry> Cache::get(const std::string &url)
{
    // TODO: look up url in entries, check TTL expiry, return if valid
    return std::nullopt;
}

void Cache::put(const std::string &url, const HttpResponse &response)
{
    // TODO: create CacheEntry with current timestamp, add to entries, evict oldest if over maxSize
}

void Cache::remove(const std::string &url)
{
    // TODO: remove entry matching url from entries
}

void Cache::clear()
{
    entries.clear();
}
