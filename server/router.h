#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace tunnelpp::server
{
    std::string host_without_port(std::string_view host);
    std::optional<std::string> extract_subdomain(std::string_view host_header, const std::string& domain);
}
