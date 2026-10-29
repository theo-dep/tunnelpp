#include <catch2/catch_test_macros.hpp>

#include <router.h>

TEST_CASE("host_without_port")
{
    using tunnelpp::server::host_without_port;
    REQUIRE(host_without_port("example.com") == "example.com");
    REQUIRE(host_without_port("example.com:8080") == "example.com");
    REQUIRE(host_without_port("[::1]:8080") == "[::1]");
    REQUIRE(host_without_port("[::1]") == "[::1]");
    REQUIRE(host_without_port("127.0.0.1:8080") == "127.0.0.1");
}

TEST_CASE("extract_subdomain")
{
    using tunnelpp::server::extract_subdomain;
    const std::string domain{ "tunnel.example.com" };
    REQUIRE(extract_subdomain("my-app.tunnel.example.com", domain) == "my-app");
    REQUIRE(extract_subdomain("my-app.tunnel.example.com:8080", domain) == "my-app");
    REQUIRE(extract_subdomain("My-App.TUNNEL.Example.COM", domain) == "my-app");
}

TEST_CASE("extract_subdomain no match")
{
    using tunnelpp::server::extract_subdomain;
    const std::string domain{ "tunnel.example.com" };
    REQUIRE(!extract_subdomain("tunnel.example.com", domain));
    REQUIRE(!extract_subdomain("other.example.com", domain));
    REQUIRE(!extract_subdomain("a.b.tunnel.example.com", domain));
    REQUIRE(!extract_subdomain("", domain));
    REQUIRE(!extract_subdomain("my-app.tunnel.example.com.evil.com", domain));
}
