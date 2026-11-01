#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace tunnelpp
{
    struct ServerConfig
    {
        std::string domain{ "tunnelpp.local" };
        std::string token;
        std::string host{ "0.0.0.0" };
        int http_port{ 8080 };
        std::string public_scheme{ "http" };
        int public_port{ 0 };
        int request_timeout_seconds{ 30 };
    };

    std::optional<std::string> get_env(std::string_view name);
    bool set_env(const std::string& name, const std::string& value);
    void unset_env(const std::string& name);

    // nullopt when the configuration is invalid (missing token, bad port, ...)
    std::optional<ServerConfig> load_server_config();
}
