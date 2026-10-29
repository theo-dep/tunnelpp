#include "router.h"

#include <stringutils.h>

namespace tunnelpp::server
{
    std::string host_without_port(std::string_view host)
    {
        if (!host.empty() && host.front() == '[') {
            const auto close{ host.find(']') };
            return close == std::string_view::npos ? std::string{ host } : std::string{ host.substr(0, close + 1) };
        }
        const auto colon{ host.rfind(':') };
        if (colon != std::string_view::npos && host.find(':') == colon) {
            return std::string{ host.substr(0, colon) };
        }
        return std::string{ host };
    }

    std::optional<std::string> extract_subdomain(std::string_view host_header, const std::string& domain)
    {
        const auto host{ lower(host_without_port(host_header)) };
        const auto suffix{ "." + lower(domain) };
        if (host.size() <= suffix.size() || !host.ends_with(suffix)) {
            return std::nullopt;
        }
        const auto subdomain{ host.substr(0, host.size() - suffix.size()) };
        if (subdomain.empty() || subdomain.contains('.')) {
            return std::nullopt;
        }
        return std::string{ subdomain };
    }
}
