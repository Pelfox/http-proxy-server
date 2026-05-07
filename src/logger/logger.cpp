#include "logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

Logger::Logger(std::ostream &pipe)
    : pipe(pipe) {}

void Logger::write(LogLevel level, const std::string &message)
{
    pipe << "[" << formatTimestamp() << "] - [" << levelToString(level) << "] " << message << '\n';
    pipe.flush();
}

std::string Logger::formatTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream timestamp;
    timestamp << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return timestamp.str();
}

std::string_view Logger::levelToString(LogLevel level)
{
    switch (level)
    {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARNING";
    case LogLevel::Error:
        return "ERROR";
    }

    return "UNKNOWN";
}
