#pragma once

#include "cache/cache.hpp"
#include "filter/filter.hpp"
#include "logger/logger.hpp"
#include "http/http_request.hpp"

#include <cstdint>
#include <string>

class ProxyServer
{
public:
    /**
     * @brief Создаёт прокси-сервер и загружает правила фильтрации.
     *
     * @param port Порт, на котором сервер будет принимать подключения.
     * @param filterConfigPath Путь к JSON-файлу с правилами фильтрации.
     */
    ProxyServer(uint16_t port, const std::string &filterConfigPath);

    void start();

private:
    void processRequest(int clientSocket, HttpRequest request);

    uint16_t port;
    Cache cache;
    Filter filter;
    Logger logger;
};
