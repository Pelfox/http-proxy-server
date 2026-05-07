#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class HttpResponse
{
public:
    uint16_t statusCode = 0;
    std::map<std::string, std::string> headers;
    std::vector<uint8_t> body;

    static HttpResponse text(uint16_t statusCode, std::string_view body);

    std::vector<uint8_t> serialize() const;

    /**
     * @brief Парсит полное HTTP-сообщение ответа из сырых байтов.
     *
     * Заполняет статус-код, заголовки и тело. Бросает исключение, если блок
     * заголовков не завершён или формат статус-строки/заголовка нарушен.
     */
    void parse(const std::vector<uint8_t> &raw);
};
