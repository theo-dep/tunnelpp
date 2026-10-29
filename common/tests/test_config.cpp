#include <catch2/catch_test_macros.hpp>

#include <config.h>

namespace
{
    void clear_env()
    {
        tunnelpp::unset_env("TUNNELPP_DOMAIN");
        tunnelpp::unset_env("TUNNELPP_HOST");
        tunnelpp::unset_env("TUNNELPP_TOKEN");
        tunnelpp::unset_env("TUNNELPP_HTTP_PORT");
        tunnelpp::unset_env("TUNNELPP_ADMIN_PORT");
        tunnelpp::unset_env("TUNNELPP_ADMIN_TOKEN");
        tunnelpp::unset_env("TUNNELPP_PUBLIC_SCHEME");
        tunnelpp::unset_env("TUNNELPP_REQUEST_TIMEOUT");
    }
}

TEST_CASE("load_server_config defaults")
{
    clear_env();
    REQUIRE(tunnelpp::set_env("TUNNELPP_TOKEN", "secret"));
    const auto config{ tunnelpp::load_server_config() };
    REQUIRE(config);
    REQUIRE(config->domain == "tunnelpp.local");
    REQUIRE(config->host == "0.0.0.0");
    REQUIRE(config->token == "secret");
    REQUIRE(config->http_port == 8080);
    REQUIRE(config->admin_port == 8081);
    REQUIRE(config->public_scheme == "http");
    REQUIRE(config->request_timeout_seconds == 30);
    REQUIRE(config->admin_token.empty());
    clear_env();
}

TEST_CASE("load_server_config custom values")
{
    clear_env();
    REQUIRE(tunnelpp::set_env("TUNNELPP_TOKEN", "secret"));
    REQUIRE(tunnelpp::set_env("TUNNELPP_DOMAIN", "tunnel.example.com"));
    REQUIRE(tunnelpp::set_env("TUNNELPP_HOST", "127.0.0.1"));
    REQUIRE(tunnelpp::set_env("TUNNELPP_HTTP_PORT", "9000"));
    REQUIRE(tunnelpp::set_env("TUNNELPP_ADMIN_PORT", "9001"));
    REQUIRE(tunnelpp::set_env("TUNNELPP_ADMIN_TOKEN", "admin-secret"));
    REQUIRE(tunnelpp::set_env("TUNNELPP_PUBLIC_SCHEME", "https"));
    REQUIRE(tunnelpp::set_env("TUNNELPP_REQUEST_TIMEOUT", "10"));
    const auto config{ tunnelpp::load_server_config() };
    REQUIRE(config);
    REQUIRE(config->domain == "tunnel.example.com");
    REQUIRE(config->host == "127.0.0.1");
    REQUIRE(config->http_port == 9000);
    REQUIRE(config->admin_port == 9001);
    REQUIRE(config->admin_token == "admin-secret");
    REQUIRE(config->public_scheme == "https");
    REQUIRE(config->request_timeout_seconds == 10);
    clear_env();
}

TEST_CASE("load_server_config requires a token")
{
    clear_env();
    REQUIRE(!tunnelpp::load_server_config());
    REQUIRE(tunnelpp::set_env("TUNNELPP_TOKEN", ""));
    REQUIRE(!tunnelpp::load_server_config());
    clear_env();
}

TEST_CASE("load_server_config rejects invalid values")
{
    clear_env();
    REQUIRE(tunnelpp::set_env("TUNNELPP_TOKEN", "secret"));
    REQUIRE(tunnelpp::set_env("TUNNELPP_HTTP_PORT", "abc"));
    REQUIRE(!tunnelpp::load_server_config());
    REQUIRE(tunnelpp::set_env("TUNNELPP_HTTP_PORT", "70000"));
    REQUIRE(!tunnelpp::load_server_config());
    REQUIRE(tunnelpp::set_env("TUNNELPP_HTTP_PORT", "8080"));
    REQUIRE(tunnelpp::set_env("TUNNELPP_PUBLIC_SCHEME", "ftp"));
    REQUIRE(!tunnelpp::load_server_config());
    clear_env();
}
