#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace tunnelpp
{
    std::string base64_encode(std::string_view data);
    std::optional<std::string> base64_decode(std::string_view data);
}
