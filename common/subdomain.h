#pragma once

#include <cstddef>
#include <random>
#include <string>
#include <string_view>

namespace tunnelpp
{
    bool is_valid_subdomain(std::string_view subdomain);
    std::string random_subdomain(std::mt19937& rng, std::size_t length = 10);
}
