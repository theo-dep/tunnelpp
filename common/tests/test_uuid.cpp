#include <catch2/catch_test_macros.hpp>

#include <uuid.h>

#include <string>

TEST_CASE("make_uuid format")
{
    const auto uuid{ tunnelpp::make_uuid() };
    REQUIRE(uuid.size() == 36);
    for (std::size_t i = 0; i < uuid.size(); ++i) {
        const char c{ uuid[i] };
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            REQUIRE(c == '-');
        } else {
            REQUIRE(((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')));
        }
    }
    REQUIRE(uuid[14] == '4');
    REQUIRE((uuid[19] == '8' || uuid[19] == '9' || uuid[19] == 'a' || uuid[19] == 'b'));
}

TEST_CASE("make_uuid unique")
{
    REQUIRE(tunnelpp::make_uuid() != tunnelpp::make_uuid());
}
