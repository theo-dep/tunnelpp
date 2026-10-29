#include "url.h"

#include "stringutils.h"

namespace tunnelpp
{
    static bool parse_port(std::string_view text, int& port)
    {
        constexpr std::size_t max_digits{ 5 };
        constexpr int max_port{ 65535 };
        constexpr int decimal_base{ 10 };
        if (text.empty() || text.size() > max_digits) {
            return false;
        }
        int value{ 0 };
        for (const char c : text) {
            if (c < '0' || c > '9') {
                return false;
            }
            value = (value * decimal_base) + (c - '0');
        }
        if (value < 1 || value > max_port) {
            return false;
        }
        port = value;
        return true;
    }

    int Url::port_or_default() const
    {
        constexpr int https_port{ 443 };
        constexpr int http_port{ 80 };
        if (port != 0) {
            return port;
        }
        return (scheme == "https" || scheme == "wss") ? https_port : http_port;
    }

    std::optional<Url> parse_url(std::string_view url)
    {
        const auto scheme_end{ url.find("://") };
        if (scheme_end == std::string_view::npos || scheme_end == 0) {
            return std::nullopt;
        }
        Url result;
        result.scheme = lower(url.substr(0, scheme_end));
        const auto rest{ url.substr(scheme_end + 3) };
        const auto slash{ rest.find('/') };
        const auto authority{ rest.substr(0, slash) };
        result.path = slash == std::string_view::npos ? "/" : std::string{ rest.substr(slash) };

        if (!authority.empty() && authority.front() == '[') {
            const auto close{ authority.find(']') };
            if (close == std::string_view::npos) {
                return std::nullopt;
            }
            result.host = std::string{ authority.substr(1, close - 1) };
            if (close + 1 < authority.size()) {
                if (authority.at(close + 1) != ':' || !parse_port(authority.substr(close + 2), result.port)) {
                    return std::nullopt;
                }
            }
        } else {
            const auto colon{ authority.rfind(':') };
            if (colon != std::string_view::npos) {
                if (!parse_port(authority.substr(colon + 1), result.port)) {
                    return std::nullopt;
                }
                result.host = std::string{ authority.substr(0, colon) };
            } else {
                result.host = std::string{ authority };
            }
        }
        if (result.host.empty()) {
            return std::nullopt;
        }
        return result;
    }
}
