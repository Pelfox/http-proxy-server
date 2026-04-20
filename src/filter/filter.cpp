#include "filter.hpp"

bool Filter::isBlocked(const std::string &url) const
{
    // TODO: check if url matches any entry in the blacklist
    return false;
}

void Filter::addUrl(const std::string &url)
{
    blacklist.insert(url);
}

void Filter::removeUrl(const std::string &url)
{
    blacklist.erase(url);
}
