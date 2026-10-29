#include <catch2/catch_test_macros.hpp>

#include <subdomain.h>

#include <random>
#include <string>

TEST_CASE("is_valid_subdomain valid")
{
    REQUIRE(tunnelpp::is_valid_subdomain("abc"));
    REQUIRE(tunnelpp::is_valid_subdomain("my-app-1"));
    REQUIRE(tunnelpp::is_valid_subdomain("a1b"));
    REQUIRE(tunnelpp::is_valid_subdomain(std::string(63, 'a')));
}

TEST_CASE("is_valid_subdomain invalid")
{
    REQUIRE(!tunnelpp::is_valid_subdomain(""));
    REQUIRE(!tunnelpp::is_valid_subdomain("ab"));
    REQUIRE(!tunnelpp::is_valid_subdomain(std::string(64, 'a')));
    REQUIRE(!tunnelpp::is_valid_subdomain("Abc"));
    REQUIRE(!tunnelpp::is_valid_subdomain("-abc"));
    REQUIRE(!tunnelpp::is_valid_subdomain("abc-"));
    REQUIRE(!tunnelpp::is_valid_subdomain("a_b"));
    REQUIRE(!tunnelpp::is_valid_subdomain("a.b"));
}

TEST_CASE("random_subdomain")
{
    std::mt19937 rng{ 42 };
    const auto subdomain{ tunnelpp::random_subdomain(rng) };
    REQUIRE(subdomain.size() == 10);
    REQUIRE(tunnelpp::is_valid_subdomain(subdomain));
    const auto custom{ tunnelpp::random_subdomain(rng, 5) };
    REQUIRE(custom.size() == 5);
}
