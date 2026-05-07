#include "filter.hpp"

#include <algorithm>
#include <fstream>
#include <ranges>
#include <stdexcept>

#include <nlohmann/json.hpp>

Filter::Filter(std::string_view configPath)
{
    loadFromFile(configPath);
}

bool Filter::isBlocked(std::string_view url) const
{
    const std::string normalizedUrl = normalizeBaseUrl(url);
    return std::ranges::any_of(blacklist, [&normalizedUrl](const std::string &baseUrl)
                               { return matchesBaseUrl(normalizedUrl, baseUrl); });
}

void Filter::addUrl(std::string_view url)
{
    const std::string normalizedUrl = normalizeBaseUrl(url);
    if (!normalizedUrl.empty())
    {
        blacklist.insert(normalizedUrl);
    }
}

void Filter::removeUrl(std::string_view url)
{
    blacklist.erase(normalizeBaseUrl(url));
}

bool Filter::matchesBaseUrl(std::string_view url, std::string_view baseUrl)
{
    if (url == baseUrl)
    {
        return true;
    }
    if (!url.starts_with(baseUrl))
    {
        return false;
    }
    if (baseUrl.ends_with('/'))
    {
        return true;
    }

    const char nextCharacter = url[baseUrl.size()];
    return nextCharacter == '/' || nextCharacter == '?' || nextCharacter == '#' || nextCharacter == ':';
}

std::string Filter::normalizeBaseUrl(std::string_view url)
{
    constexpr std::string_view HttpScheme = "http://";
    constexpr std::string_view HttpsScheme = "https://";

    if (url.starts_with(HttpsScheme))
    {
        url.remove_prefix(HttpsScheme.size());
    }
    else if (url.starts_with(HttpScheme))
    {
        url.remove_prefix(HttpScheme.size());
    }

    while (url.size() > 1 && url.ends_with('/'))
    {
        url.remove_suffix(1);
    }

    return std::string(url);
}

void Filter::loadFromFile(std::string_view configPath)
{
    const std::string path(configPath);
    std::ifstream config(path);
    if (!config.is_open())
    {
        throw std::runtime_error("Unable to open filter config: " + path);
    }

    const auto rules = nlohmann::json::parse(config);
    if (!rules.is_array())
    {
        throw std::runtime_error("Filter config must contain a JSON array.");
    }

    for (const auto &rule : rules)
    {
        if (!rule.is_string())
        {
            throw std::runtime_error("Each filter rule must be a string.");
        }

        const auto baseUrl = rule.get<std::string>();
        if (baseUrl.empty())
        {
            throw std::runtime_error("Filter rule must not be empty.");
        }

        addUrl(baseUrl);
    }
}
