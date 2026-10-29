#include <catch2/catch_test_macros.hpp>

#include <args.h>

using tunnelpp::client::parse_options;

namespace
{
    std::vector<std::string> args(std::initializer_list<const char*> values)
    {
        return { values.begin(), values.end() };
    }
}

TEST_CASE("parse_options valid")
{
    const auto options{ parse_options(args({
        "tunnelpp-client",
        "--server",
        "ws://127.0.0.1:8080/tunnel",
        "--token",
        "secret",
        "--local",
        "http://127.0.0.1:3000",
        "--subdomain",
        "my-app",
        "--max-reconnect-delay",
        "5",
        "--verbose",
    })) };
    REQUIRE(options);
    REQUIRE(options->server_url == "ws://127.0.0.1:8080/tunnel");
    REQUIRE(options->token == "secret");
    REQUIRE(options->local.scheme == "http");
    REQUIRE(options->local.host == "127.0.0.1");
    REQUIRE(options->local.port == 3000);
    REQUIRE(options->subdomain == "my-app");
    REQUIRE(options->max_reconnect_delay_seconds == 5);
    REQUIRE(options->verbose);
}

TEST_CASE("parse_options defaults")
{
    const auto options{ parse_options(args({
        "tunnelpp-client",
        "--server",
        "wss://tunnel.example.com/tunnel",
        "--token",
        "secret",
    })) };
    REQUIRE(options);
    REQUIRE(options->local.scheme == "http");
    REQUIRE(options->local.host == "127.0.0.1");
    REQUIRE(options->local.port == 8080);
    REQUIRE(options->subdomain.empty());
    REQUIRE(options->max_reconnect_delay_seconds == 30);
    REQUIRE(!options->verbose);
}

TEST_CASE("parse_options rejects invalid input")
{
    REQUIRE(!parse_options(args({ "tunnelpp-client", "--server", "http://x/tunnel", "--token", "s" })));
    REQUIRE(!parse_options(args({ "tunnelpp-client", "--server", "ws://x/tunnel", "--token", "s", "--local", "ftp://x" })));
    REQUIRE(!parse_options(args({ "tunnelpp-client", "--server", "ws://x/tunnel", "--token", "s", "--subdomain", "AB" })));
    REQUIRE(!parse_options(args({ "tunnelpp-client", "--server", "ws://x/tunnel" }))); // missing --token
}
