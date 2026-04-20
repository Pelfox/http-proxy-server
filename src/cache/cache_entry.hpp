#pragma once

#include "http/http_response.hpp"

#include <chrono>
#include <string>

struct CacheEntry
{
    std::string url;
    HttpResponse response;
    std::chrono::system_clock::time_point createdAt;
};
