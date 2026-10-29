#include <catch2/catch_test_macros.hpp>

#include <base64.h>

TEST_CASE("base64_encode empty")
{
    REQUIRE(tunnelpp::base64_encode("") == "");
}

TEST_CASE("base64_encode known vectors")
{
    REQUIRE(tunnelpp::base64_encode("f") == "Zg==");
    REQUIRE(tunnelpp::base64_encode("fo") == "Zm8=");
    REQUIRE(tunnelpp::base64_encode("foo") == "Zm9v");
    REQUIRE(tunnelpp::base64_encode("foob") == "Zm9vYg==");
    REQUIRE(tunnelpp::base64_encode("fooba") == "Zm9vYmE=");
    REQUIRE(tunnelpp::base64_encode("foobar") == "Zm9vYmFy");
}

TEST_CASE("base64_decode known vectors")
{
    REQUIRE(tunnelpp::base64_decode("Zg==") == "f");
    REQUIRE(tunnelpp::base64_decode("Zm8=") == "fo");
    REQUIRE(tunnelpp::base64_decode("Zm9v") == "foo");
    REQUIRE(tunnelpp::base64_decode("Zm9vYmFy") == "foobar");
    REQUIRE(tunnelpp::base64_decode("") == "");
}

TEST_CASE("base64 round-trip binary")
{
    std::string binary;
    for (int i = 0; i < 256; ++i) {
        binary.push_back(static_cast<char>(i));
    }
    REQUIRE(tunnelpp::base64_decode(tunnelpp::base64_encode(binary)) == binary);
}

TEST_CASE("base64_decode invalid")
{
    REQUIRE(!tunnelpp::base64_decode("Zg=").has_value());
    REQUIRE(!tunnelpp::base64_decode("Zg===").has_value());
    REQUIRE(!tunnelpp::base64_decode("Zm9").has_value());
    REQUIRE(!tunnelpp::base64_decode("Zm9v!bcd").has_value());
    REQUIRE(!tunnelpp::base64_decode(" Zm9v").has_value());
    REQUIRE(!tunnelpp::base64_decode("Zm9v\n").has_value());
}
