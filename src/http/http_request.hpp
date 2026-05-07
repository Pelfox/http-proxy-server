#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class HttpRequest
{
public:
    std::string method;
    std::string url;
    std::string version;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    /**
     * @brief Парсит полное HTTP-сообщение из сырых байтов.
     *
     * Заполняет request-line, заголовки и тело. Бросает исключение, если
     * заголовки повреждены или тело короче объявленного Content-Length.
     */
    void parse(const std::vector<uint8_t> &raw);

    std::optional<std::string> headerValue(std::string_view name) const;
    std::size_t contentLength() const;

    /**
     * @brief Возвращает Content-Length из блока заголовков HTTP-сообщения.
     *
     * Используется при поточном чтении из сокета, когда нужно понять, сколько
     * байт тела ещё предстоит дочитать после получения заголовков.
     *
     * @param headerBlock Часть сообщения от начала до (но не включая) "\r\n\r\n".
     */
    static std::size_t contentLengthFromHeaders(std::string_view headerBlock);
};
