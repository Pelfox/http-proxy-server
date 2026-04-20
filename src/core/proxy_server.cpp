#include "proxy_server.hpp"

#include <iostream>

ProxyServer::ProxyServer(uint16_t port)
    : port(port), cache(100, 300), filter(), logger(std::cerr) {}

void ProxyServer::start()
{
    // TODO: bind socket to port, listen, accept connections in loop
}

void ProxyServer::stop()
{
    // TODO: close listening socket, clean up resources
}

HttpResponse ProxyServer::handleRequest(const HttpRequest &request)
{
    // TODO: implement request processing pipeline:
    //   1. Log incoming request
    //   2. Filter - if filter.isBlocked(request.url), return 403 response
    //   3. Cache - if cache hit, return cached response
    //   4. Forward request to target server
    //   5. Store response in cache
    //   6. Return response to client
    return {};
}
