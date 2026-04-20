#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

class HttpResponse
{
public:
    uint16_t statusCode = 0;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    std::vector<uint8_t> serialize() const;
};
