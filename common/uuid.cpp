#include "uuid.h"

#include <random>

namespace tunnelpp
{
    std::string make_uuid()
    {
        thread_local std::mt19937 rng{ std::random_device{}() };
        constexpr std::string_view hex{ "0123456789abcdef" };
        std::uniform_int_distribution<std::size_t> dist{ 0, hex.size() - 1 };
        std::string uuid{ "00000000-0000-4000-8000-000000000000" };
        for (char& c : uuid) {
            if (c == '0') {
                c = hex.at(dist(rng));
            }
        }
        return uuid;
    }
}
