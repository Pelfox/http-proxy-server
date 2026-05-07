#include "core/proxy_server.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

namespace
{
    constexpr uint16_t ProxyPort = 8080;
    const std::string FilterConfigPath = "filter.json";
}

int main()
{
    try
    {
        ProxyServer server(ProxyPort, FilterConfigPath);
        server.start();
    }
    catch (const std::exception &error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
