#include <catch2/catch_test_macros.hpp>

#include <url.h>

TEST_CASE("parse_url full")
{
    const auto url{ tunnelpp::parse_url("ws://example.com:8080/tunnel?x=1") };
    REQUIRE(url);
    REQUIRE(url->scheme == "ws");
    REQUIRE(url->host == "example.com");
    REQUIRE(url->port == 8080);
    REQUIRE(url->path == "/tunnel?x=1");
}

TEST_CASE("parse_url defaults")
{
    const auto url{ tunnelpp::parse_url("http://127.0.0.1") };
    REQUIRE(url);
    REQUIRE(url->scheme == "http");
    REQUIRE(url->host == "127.0.0.1");
    REQUIRE(url->port == 0);
    REQUIRE(url->path == "/");
    REQUIRE(url->port_or_default() == 80);

    const auto secure{ tunnelpp::parse_url("wss://example.com/tunnel") };
    REQUIRE(secure);
    REQUIRE(secure->port_or_default() == 443);
}

TEST_CASE("parse_url ipv6")
{
    const auto url{ tunnelpp::parse_url("http://[::1]:9000/x") };
    REQUIRE(url);
    REQUIRE(url->host == "::1");
    REQUIRE(url->port == 9000);
    REQUIRE(url->path == "/x");
}

TEST_CASE("parse_url invalid")
{
    REQUIRE(!tunnelpp::parse_url(""));
    REQUIRE(!tunnelpp::parse_url("example.com"));
    REQUIRE(!tunnelpp::parse_url("://example.com"));
    REQUIRE(!tunnelpp::parse_url("http://"));
    REQUIRE(!tunnelpp::parse_url("http://host:abc/"));
    REQUIRE(!tunnelpp::parse_url("http://host:99999/"));
    REQUIRE(!tunnelpp::parse_url("http://host:0/"));
    REQUIRE(!tunnelpp::parse_url("http://[::1/x"));
}
