#include "logger.hpp"

Logger::Logger(std::ostream &pipe)
    : pipe(pipe) {}

void Logger::write(int level, const std::string &message)
{
    // TODO: format and write log message with level and timestamp to pipe
}
