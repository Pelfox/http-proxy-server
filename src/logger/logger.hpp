#pragma once

#include <ostream>
#include <string>

class Logger
{
public:
    explicit Logger(std::ostream &pipe);

    void write(int level, const std::string &message);

private:
    std::ostream &pipe;
};
