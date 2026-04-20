#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class HttpRequest
{
public:
    std::string method;
    std::string url;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    void parse();
};
