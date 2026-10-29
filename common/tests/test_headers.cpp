#include <catch2/catch_test_macros.hpp>

#include <headers.h>

TEST_CASE("filter_tunnel_headers removes hop-by-hop and framing headers")
{
    const httplib::Headers input{
        { "Host", "example.com" },
        { "Connection", "keep-alive" },
        { "Transfer-Encoding", "chunked" },
        { "Content-Length", "42" },
        { "Upgrade", "websocket" },
        { "Accept-Encoding", "gzip, deflate, br" },
        { "Content-Type", "application/json" },
        { "X-Custom", "yes" },
        { "Set-Cookie", "a=1" },
        { "set-cookie", "b=2" },
    };
    const auto output{ tunnelpp::filter_tunnel_headers(input) };
    REQUIRE(output.size() == 4);
    REQUIRE(output.count("Content-Type") == 1);
    REQUIRE(output.count("X-Custom") == 1);
    REQUIRE(output.count("Set-Cookie") == 2); // keys are case-insensitive: both spellings are kept
    REQUIRE(output.count("set-cookie") == 2);
    REQUIRE(output.count("Host") == 0);
    REQUIRE(output.count("Connection") == 0);
    REQUIRE(output.count("Transfer-Encoding") == 0);
    REQUIRE(output.count("Content-Length") == 0);
    REQUIRE(output.count("Upgrade") == 0);
    REQUIRE(output.count("Accept-Encoding") == 0);
}

TEST_CASE("filter_tunnel_headers is case-insensitive")
{
    const httplib::Headers input{
        { "HOST", "example.com" },
        { "content-LENGTH", "42" },
        { "X-Keep", "1" },
    };
    const auto output{ tunnelpp::filter_tunnel_headers(input) };
    REQUIRE(output.size() == 1);
    REQUIRE(output.count("X-Keep") == 1);
}
