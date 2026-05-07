#include "cache.hpp"

#include <algorithm>

Cache::Cache(std::size_t maxSize, std::chrono::seconds defaultTTL)
    : maxSize(maxSize), defaultTTL(defaultTTL) {}

std::optional<CacheEntry> Cache::get(const std::string &url)
{
    std::lock_guard<std::mutex> lock(mutex);
    const auto it = std::find_if(entries.begin(), entries.end(),
                                 [&](const CacheEntry &entry)
                                 { return entry.url == url; });
    if (it == entries.end())
    {
        return std::nullopt;
    }

    if (isExpired(*it))
    {
        entries.erase(it);
        return std::nullopt;
    }

    if (it != entries.begin())
    {
        entries.splice(entries.begin(), entries, it);
    }
    return entries.front();
}

void Cache::put(const std::string &url, const HttpResponse &response)
{
    std::lock_guard<std::mutex> lock(mutex);
    entries.remove_if([&](const CacheEntry &entry)
                      { return entry.url == url; });

    entries.push_front(CacheEntry{url, response, std::chrono::system_clock::now()});
    while (entries.size() > maxSize)
    {
        entries.pop_back();
    }
}

void Cache::remove(const std::string &url)
{
    std::lock_guard<std::mutex> lock(mutex);
    entries.remove_if([&](const CacheEntry &entry)
                      { return entry.url == url; });
}

void Cache::clear()
{
    std::lock_guard<std::mutex> lock(mutex);
    entries.clear();
}

bool Cache::isExpired(const CacheEntry &entry) const
{
    const auto age = std::chrono::system_clock::now() - entry.createdAt;
    return age > defaultTTL;
}
