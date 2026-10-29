#include "headers.h"

#include "stringutils.h"

#include <algorithm>
#include <array>
#include <string_view>

namespace tunnelpp
{
    httplib::Headers filter_tunnel_headers(const httplib::Headers& headers)
    {
        constexpr std::array<std::string_view, 12> excluded{
            "host",
            "connection",
            "keep-alive",
            "proxy-authenticate",
            "proxy-authorization",
            "proxy-connection",
            "te",
            "trailer",
            "transfer-encoding",
            "upgrade",
            "content-length",
            "accept-encoding",
        };
        httplib::Headers result;
        for (const auto& [key, value] : headers) {
            const auto name{ lower(key) };
            if (!std::ranges::contains(excluded, name)) {
                result.emplace(key, value);
            }
        }
        return result;
    }
}
