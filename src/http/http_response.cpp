#include "http_response.hpp"

#include <string>
#include <string_view>

namespace
{
    std::string_view reasonPhrase(uint16_t statusCode)
    {
        switch (statusCode)
        {
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 204:
            return "No Content";
        case 301:
            return "Moved Permanently";
        case 302:
            return "Found";
        case 400:
            return "Bad Request";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 500:
            return "Internal Server Error";
        case 502:
            return "Bad Gateway";
        case 503:
            return "Service Unavailable";
        default:
            return "Unknown";
        }
    }

    void append(std::vector<uint8_t> &target, std::string_view value)
    {
        target.insert(target.end(), value.begin(), value.end());
    }
}

HttpResponse HttpResponse::text(uint16_t statusCode, std::string_view body)
{
    HttpResponse response;
    response.statusCode = statusCode;
    response.headers["Content-Type"] = "text/plain";
    response.body.assign(body.begin(), body.end());
    return response;
}

std::vector<uint8_t> HttpResponse::serialize() const
{
    std::vector<uint8_t> result;

    const auto statusLine = "HTTP/1.1 " + std::to_string(statusCode) + " " + std::string(reasonPhrase(statusCode)) + "\r\n";
    append(result, statusLine);

    for (const auto &[name, value] : headers)
    {
        append(result, name);
        append(result, ": ");
        append(result, value);
        append(result, "\r\n");
    }

    if (!headers.contains("Content-Length"))
    {
        append(result, "Content-Length: ");
        append(result, std::to_string(body.size()));
        append(result, "\r\n");
    }

    append(result, "\r\n");
    result.insert(result.end(), body.begin(), body.end());

    return result;
}
