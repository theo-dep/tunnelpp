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
        int admin_port{ 8081 };
        std::string admin_token;
        std::string public_scheme{ "http" };
        int request_timeout_seconds{ 30 };
    };

    std::optional<std::string> get_env(std::string_view name);
    bool set_env(const std::string& name, const std::string& value);
    void unset_env(const std::string& name);

    // nullopt when the configuration is invalid (missing token, bad port, ...)
    std::optional<ServerConfig> load_server_config();
}
