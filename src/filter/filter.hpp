#pragma once

#include <set>
#include <string>

class Filter
{
public:
    bool isBlocked(const std::string &url) const;
    void addUrl(const std::string &url);
    void removeUrl(const std::string &url);

private:
    std::set<std::string> blacklist;
};
