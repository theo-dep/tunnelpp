#include "subdomain.h"

#include <algorithm>

namespace tunnelpp
{
    static bool is_alnum(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    }

    bool is_valid_subdomain(std::string_view subdomain)
    {
        constexpr std::size_t max_length{ 63 };
        if (subdomain.size() < 3 || subdomain.size() > max_length) {
            return false;
        }
        if (!is_alnum(subdomain.front()) || !is_alnum(subdomain.back())) {
            return false;
        }
        return std::ranges::all_of(subdomain, [](char c) { return is_alnum(c) || c == '-'; });
    }

    std::string random_subdomain(std::mt19937& rng, std::size_t length)
    {
        constexpr std::string_view chars{ "abcdefghijklmnopqrstuvwxyz0123456789" };
        std::uniform_int_distribution<std::size_t> dist{ 0, chars.size() - 1 };
        std::string result(length, 'a');
        for (char& c : result) {
            c = chars.at(dist(rng));
        }
        return result;
    }
}
