#include "config.h"

#include <cstdlib>

namespace tunnelpp
{
    static bool env_int(const char* name, int& target)
    {
        constexpr int max_port{ 65535 };
        const auto value{ get_env(name) };
        if (!value) {
            return true;
        }
        try {
            std::size_t pos{ 0 };
            const int parsed{ std::stoi(*value, &pos) };
            if (pos != value->size() || parsed < 1 || parsed > max_port) {
                return false;
            }
            target = parsed;
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

    std::optional<std::string> get_env(std::string_view name)
    {
        const std::string key{ name };
#ifdef _WIN32
        // std::getenv is deprecated on the UCRT (_CRT_INSECURE_DEPRECATE)
        char* value{ nullptr };
        std::size_t size{ 0 };
        if (_dupenv_s(&value, &size, key.c_str()) != 0 || value == nullptr) {
            return std::nullopt;
        }
        std::string result{ value };
        std::free(value);
        return result;
#else
        if (const char* value = std::getenv(key.c_str())) {
            return std::string{ value };
        }
        return std::nullopt;
#endif
    }

    bool set_env(const std::string& name, const std::string& value)
    {
#ifdef _WIN32
        return _putenv_s(name.c_str(), value.c_str()) == 0;
#else
        return ::setenv(name.c_str(), value.c_str(), 1) == 0;
#endif
    }

    void unset_env(const std::string& name)
    {
#ifdef _WIN32
        _putenv_s(name.c_str(), "");
#else
        ::unsetenv(name.c_str());
#endif
    }

    std::optional<ServerConfig> load_server_config()
    {
        ServerConfig config;
        if (const auto value = get_env("TUNNELPP_DOMAIN")) {
            config.domain = *value;
        }
        if (const auto value = get_env("TUNNELPP_HOST")) {
            config.host = *value;
        }
        if (const auto value = get_env("TUNNELPP_ADMIN_TOKEN")) {
            config.admin_token = *value;
        }
        if (const auto value = get_env("TUNNELPP_PUBLIC_SCHEME")) {
            config.public_scheme = *value;
        }
        if (!env_int("TUNNELPP_HTTP_PORT", config.http_port)) {
            return std::nullopt;
        }
        if (!env_int("TUNNELPP_ADMIN_PORT", config.admin_port)) {
            return std::nullopt;
        }
        if (!env_int("TUNNELPP_REQUEST_TIMEOUT", config.request_timeout_seconds)) {
            return std::nullopt;
        }
        if (const auto value = get_env("TUNNELPP_TOKEN")) {
            config.token = *value;
        }
        if (config.token.empty()) {
            return std::nullopt;
        }
        if (config.public_scheme != "http" && config.public_scheme != "https") {
            return std::nullopt;
        }
        return config;
    }
}
