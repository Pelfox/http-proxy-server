#include "http_request.hpp"

#include "utils/string_utils.hpp"

#include <charconv>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    constexpr std::string_view HeaderSeparator = "\r\n\r\n";
    constexpr std::string_view LineSeparator = "\r\n";
    constexpr std::string_view ContentLengthHeader = "Content-Length";

    std::size_t parseContentLength(std::string_view value)
    {
        std::size_t contentLength = 0;
        const auto result = std::from_chars(value.data(), value.data() + value.size(), contentLength);

        if (result.ec != std::errc() || result.ptr != value.data() + value.size())
        {
            throw std::runtime_error("Invalid Content-Length header.");
        }

        return contentLength;
    }

    void parseRequestLine(std::string_view requestLine, HttpRequest &request)
    {
        std::istringstream stream{std::string(requestLine)};
        std::string extraToken;

        if (!(stream >> request.method >> request.url >> request.version) || stream >> extraToken)
        {
            throw std::runtime_error("Invalid HTTP request line.");
        }
    }

    /**
     * @brief Парсит блок заголовков (без request-line) в map.
     *
     * Общая часть для полного парсинга и для извлечения отдельных значений.
     */
    void parseHeaderLines(std::string_view block, std::map<std::string, std::string> &headers)
    {
        std::size_t lineStart = 0;
        while (lineStart < block.size())
        {
            const auto lineEnd = block.find(LineSeparator, lineStart);
            const auto line = lineEnd == std::string_view::npos
                                  ? block.substr(lineStart)
                                  : block.substr(lineStart, lineEnd - lineStart);

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

    std::string_view headersAfterRequestLine(std::string_view headerBlock)
    {
        const auto requestLineEnd = headerBlock.find(LineSeparator);
        if (requestLineEnd == std::string_view::npos)
        {
            return {};
        }
        return headerBlock.substr(requestLineEnd + LineSeparator.size());
    }
}

void HttpRequest::parse(const std::vector<uint8_t> &raw)
{
    const std::string_view rawView(reinterpret_cast<const char *>(raw.data()), raw.size());

    const auto headerEnd = rawView.find(HeaderSeparator);
    if (headerEnd == std::string_view::npos)
    {
        throw std::runtime_error("HTTP request does not contain a complete header block.");
    }

    const auto headerBlock = rawView.substr(0, headerEnd);
    const auto bodyBlock = rawView.substr(headerEnd + HeaderSeparator.size());

    const auto requestLineEnd = headerBlock.find(LineSeparator);
    if (requestLineEnd == std::string_view::npos)
    {
        throw std::runtime_error("HTTP request line is missing.");
    }

    parseRequestLine(headerBlock.substr(0, requestLineEnd), *this);
    headers.clear();
    parseHeaderLines(headerBlock.substr(requestLineEnd + LineSeparator.size()), headers);

    const auto declared = headerValue(ContentLengthHeader);
    const auto expected = declared.has_value() ? parseContentLength(*declared) : bodyBlock.size();
    if (bodyBlock.size() < expected)
    {
        throw std::runtime_error("HTTP request body is shorter than Content-Length.");
    }

    body.assign(bodyBlock.begin(), bodyBlock.begin() + static_cast<std::ptrdiff_t>(expected));
}

std::optional<std::string> HttpRequest::headerValue(std::string_view name) const
{
    const auto expected = string_utils::toLower(name);

    for (const auto &[headerName, value] : headers)
    {
        if (string_utils::toLower(headerName) == expected)
        {
            return value;
        }
    }

    return std::nullopt;
}

std::size_t HttpRequest::contentLength() const
{
    const auto value = headerValue(ContentLengthHeader);
    return value.has_value() ? parseContentLength(*value) : 0;
}

std::size_t HttpRequest::contentLengthFromHeaders(std::string_view headerBlock)
{
    const auto block = headersAfterRequestLine(headerBlock);
    if (block.empty())
    {
        return 0;
    }

    HttpRequest probe;
    parseHeaderLines(block, probe.headers);
    return probe.contentLength();
}
