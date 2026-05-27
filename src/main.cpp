#include "core/proxy_server.hpp"

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{
    constexpr uint16_t ProxyPort = 8080;
    const std::string FilterConfigPath = "filter.json";

    struct Options
    {
        uint16_t port = ProxyPort;
        std::string filterConfigPath = FilterConfigPath;
        bool showHelp = false;
    };

    void printUsage(std::ostream &out, std::string_view executable)
    {
        out << "Usage: " << executable << " [--port PORT] [--filter PATH]\n"
            << "\n"
            << "Options:\n"
            << "  -p, --port PORT     TCP port to listen on (default: " << ProxyPort << ")\n"
            << "  -f, --filter PATH   Path to filter JSON file (default: " << FilterConfigPath << ")\n"
            << "  -h, --help          Show this help message\n";
    }

    uint16_t parsePort(std::string_view value)
    {
        unsigned int parsed = 0;
        const auto *begin = value.data();
        const auto *end = begin + value.size();
        const auto [position, error] = std::from_chars(begin, end, parsed);
        if (error != std::errc() || position != end || parsed == 0 || parsed > 65535)
        {
            throw std::runtime_error("Port must be a number from 1 to 65535.");
        }

        return static_cast<uint16_t>(parsed);
    }

    std::string_view nextArgument(int &index, int argc, char *argv[], std::string_view option)
    {
        if (index + 1 >= argc)
        {
            throw std::runtime_error("Missing value for " + std::string(option) + ".");
        }

        ++index;
        return argv[index];
    }

    Options parseOptions(int argc, char *argv[])
    {
        Options options;
        for (int i = 1; i < argc; ++i)
        {
            const std::string_view argument(argv[i]);
            if (argument == "-h" || argument == "--help")
            {
                options.showHelp = true;
            }
            else if (argument == "-p" || argument == "--port")
            {
                options.port = parsePort(nextArgument(i, argc, argv, argument));
            }
            else if (argument.starts_with("--port="))
            {
                options.port = parsePort(argument.substr(std::string_view("--port=").size()));
            }
            else if (argument == "-f" || argument == "--filter")
            {
                options.filterConfigPath = std::string(nextArgument(i, argc, argv, argument));
            }
            else if (argument.starts_with("--filter="))
            {
                options.filterConfigPath = std::string(argument.substr(std::string_view("--filter=").size()));
            }
            else
            {
                throw std::runtime_error("Unknown argument: " + std::string(argument));
            }
        }

        return options;
    }
}

int main(int argc, char *argv[])
{
    try
    {
        const auto options = parseOptions(argc, argv);
        if (options.showHelp)
        {
            printUsage(std::cout, argv[0]);
            return 0;
        }

        ProxyServer server(options.port, options.filterConfigPath);
        server.start();
    }
    catch (const std::exception &error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        printUsage(std::cerr, argv[0]);
        return 1;
    }

    return 0;
}
