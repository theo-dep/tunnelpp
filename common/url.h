#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace tunnelpp
{
    struct Url
    {
        std::string scheme;
        std::string host;
        int port{ 0 };    // 0 means the scheme default
        std::string path; // "/" when empty, includes the query string

        int port_or_default() const;
    };

    std::optional<Url> parse_url(std::string_view url);
}
