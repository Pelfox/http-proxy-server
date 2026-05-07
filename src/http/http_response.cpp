#include "http_response.hpp"

#include "utils/string_utils.hpp"

#include <charconv>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

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

namespace
{
    constexpr std::string_view HeaderSeparator = "\r\n\r\n";
    constexpr std::string_view LineSeparator = "\r\n";
}

void HttpResponse::parse(const std::vector<uint8_t> &raw)
{
    const std::string_view view(reinterpret_cast<const char *>(raw.data()), raw.size());

    const auto headerEnd = view.find(HeaderSeparator);
    if (headerEnd == std::string_view::npos)
    {
        throw std::runtime_error("HTTP response does not contain a complete header block.");
    }

    const auto headerBlock = view.substr(0, headerEnd);
    const auto bodyBlock = view.substr(headerEnd + HeaderSeparator.size());

    const auto statusLineEnd = headerBlock.find(LineSeparator);
    const auto statusLine = statusLineEnd == std::string_view::npos
                                ? headerBlock
                                : headerBlock.substr(0, statusLineEnd);

    const auto firstSpace = statusLine.find(' ');
    if (firstSpace == std::string_view::npos)
    {
        throw std::runtime_error("Invalid HTTP status line.");
    }

    const auto codeStart = firstSpace + 1;
    const auto secondSpace = statusLine.find(' ', codeStart);
    const auto codeStr = secondSpace == std::string_view::npos
                             ? statusLine.substr(codeStart)
                             : statusLine.substr(codeStart, secondSpace - codeStart);

    uint16_t code = 0;
    const auto result = std::from_chars(codeStr.data(), codeStr.data() + codeStr.size(), code);
    if (result.ec != std::errc() || result.ptr != codeStr.data() + codeStr.size())
    {
        throw std::runtime_error("Invalid HTTP status code.");
    }
    statusCode = code;

    headers.clear();
    if (statusLineEnd != std::string_view::npos)
    {
        const auto rest = headerBlock.substr(statusLineEnd + LineSeparator.size());
        std::size_t lineStart = 0;
        while (lineStart < rest.size())
        {
            const auto lineEnd = rest.find(LineSeparator, lineStart);
            const auto line = lineEnd == std::string_view::npos
                                  ? rest.substr(lineStart)
                                  : rest.substr(lineStart, lineEnd - lineStart);

            const auto delimiter = line.find(':');
            if (delimiter == std::string_view::npos)
            {
                throw std::runtime_error("Invalid HTTP header line.");
            }

            auto name = string_utils::trim(line.substr(0, delimiter));
            if (name.empty())
            {
                throw std::runtime_error("HTTP header name must not be empty.");
            }

            headers[std::move(name)] = string_utils::trim(line.substr(delimiter + 1));

            if (lineEnd == std::string_view::npos)
            {
                break;
            }
            lineStart = lineEnd + LineSeparator.size();
        }
    }

    body.assign(bodyBlock.begin(), bodyBlock.end());
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
