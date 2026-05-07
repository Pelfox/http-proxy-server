#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace string_utils
{
    inline std::string toLower(std::string_view value)
    {
        std::string result(value);
        std::ranges::transform(result, result.begin(), [](unsigned char character)
                               { return static_cast<char>(std::tolower(character)); });
        return result;
    }

    inline std::string trim(std::string_view value)
    {
        const auto first = value.find_first_not_of(" \t");
        if (first == std::string_view::npos)
        {
            return {};
        }

        const auto last = value.find_last_not_of(" \t");
        return std::string(value.substr(first, last - first + 1));
    }
}
