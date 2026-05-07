#pragma once

#include <ostream>
#include <string>

/**
 * @brief Уровень важности логируемого сообщения.
 */
enum class LogLevel
{
    /// Подробная диагностическая информация.
    Debug,

    /// Общая информация о ходе выполнения программы.
    Info,

    /// Потенциальная проблема, не останавливающая выполнение программы.
    Warning,

    /// Ошибка, из-за которой операция не может быть успешно завершена.
    Error
};

/**
 * @brief Записывает форматированные сообщения лога в поток вывода.
 */
class Logger
{
public:
    /**
     * @brief Создаёт логгер, записывающий сообщения в переданный поток.
     *
     * @param pipe Поток вывода, используемый как место записи логов.
     */
    explicit Logger(std::ostream &pipe);

    /**
     * @brief Записывает одно форматированное сообщение лога.
     *
     * @param level Уровень важности сообщения.
     * @param message Текст сообщения.
     */
    void write(LogLevel level, const std::string &message);

private:
    /**
     * @brief Возвращает текущее локальное время в виде строки timestamp.
     */
    static std::string formatTimestamp();

    /**
     * @brief Преобразует уровень логирования в текстовое представление.
     *
     * @param level Уровень логирования для преобразования.
     */
    static std::string levelToString(LogLevel level);

    std::ostream &pipe;
};
