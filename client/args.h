#pragma once

#include <optional>
#include <string>
#include <vector>

#include <url.h>

namespace tunnelpp::client
{
    struct ClientOptions
    {
        std::string server_url;
        std::string token;
        Url local;
        std::string subdomain; // empty = random
        int max_reconnect_delay_seconds{ 30 };
        bool verbose{ false };
    };

    std::optional<ClientOptions> parse_options(const std::vector<std::string>& args);
}
