#pragma once

#include "cache/cache.hpp"
#include "filter/filter.hpp"
#include "logger/logger.hpp"
#include "http/http_request.hpp"
#include "http/http_response.hpp"

#include <cstdint>

class ProxyServer
{
public:
    explicit ProxyServer(uint16_t port);

    void start();
    void stop();
    HttpResponse handleRequest(const HttpRequest &request);

private:
    uint16_t port;
    Cache cache;
    Filter filter;
    Logger logger;
};
