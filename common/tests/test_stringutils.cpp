#include <catch2/catch_test_macros.hpp>

#include <stringutils.h>

TEST_CASE("lower")
{
    REQUIRE(tunnelpp::lower("Hello WORLD") == "hello world");
    REQUIRE(tunnelpp::lower("") == "");
    REQUIRE(tunnelpp::lower("abc") == "abc");
    REQUIRE(tunnelpp::lower("A1-B_") == "a1-b_");
}
