#include <catch2/catch_test_macros.hpp>

#include <protocol.h>

TEST_CASE("protocol register round-trip")
{
    tunnelpp::Message message;
    message.type = tunnelpp::MessageType::Register;
    message.version = tunnelpp::protocol_version;
    message.subdomain = "my-app";
    message.token = "secret";

    const auto decoded{ tunnelpp::decode_message(tunnelpp::encode_message(message)) };
    REQUIRE(decoded);
    REQUIRE(decoded->type == tunnelpp::MessageType::Register);
    REQUIRE(decoded->version == tunnelpp::protocol_version);
    REQUIRE(decoded->subdomain == "my-app");
    REQUIRE(decoded->token == "secret");
}

TEST_CASE("protocol registered round-trip")
{
    tunnelpp::Message message;
    message.type = tunnelpp::MessageType::Registered;
    message.subdomain = "my-app";
    message.url = "http://my-app.tunnel.example.com:8080";

    const auto decoded{ tunnelpp::decode_message(tunnelpp::encode_message(message)) };
    REQUIRE(decoded);
    REQUIRE(decoded->type == tunnelpp::MessageType::Registered);
    REQUIRE(decoded->subdomain == "my-app");
    REQUIRE(decoded->url == "http://my-app.tunnel.example.com:8080");
}

TEST_CASE("protocol request round-trip with binary body and duplicate headers")
{
    tunnelpp::Message message;
    message.type = tunnelpp::MessageType::Request;
    message.id = "id-1";
    message.method = "POST";
    message.path = "/api?x=1";
    message.headers.emplace("Content-Type", "application/octet-stream");
    message.headers.emplace("X-Multi", "a");
    message.headers.emplace("X-Multi", "b");
    message.body = std::string{ "a\0b\nc", 5 };

    const auto decoded{ tunnelpp::decode_message(tunnelpp::encode_message(message)) };
    REQUIRE(decoded);
    REQUIRE(decoded->type == tunnelpp::MessageType::Request);
    REQUIRE(decoded->id == "id-1");
    REQUIRE(decoded->method == "POST");
    REQUIRE(decoded->path == "/api?x=1");
    REQUIRE(decoded->headers.count("X-Multi") == 2);
    REQUIRE(decoded->body == message.body);
}

TEST_CASE("protocol response round-trip")
{
    tunnelpp::Message message;
    message.type = tunnelpp::MessageType::Response;
    message.id = "id-1";
    message.status = 201;
    message.headers.emplace("Set-Cookie", "a=1");
    message.headers.emplace("Set-Cookie", "b=2");
    message.body = "created";

    const auto decoded{ tunnelpp::decode_message(tunnelpp::encode_message(message)) };
    REQUIRE(decoded);
    REQUIRE(decoded->type == tunnelpp::MessageType::Response);
    REQUIRE(decoded->id == "id-1");
    REQUIRE(decoded->status == 201);
    REQUIRE(decoded->headers.count("Set-Cookie") == 2);
    REQUIRE(decoded->body == "created");
}

TEST_CASE("protocol error round-trip")
{
    tunnelpp::Message message;
    message.type = tunnelpp::MessageType::Error;
    message.id = "id-9";
    message.code = "invalid_token";
    message.detail = "invalid token";

    const auto decoded{ tunnelpp::decode_message(tunnelpp::encode_message(message)) };
    REQUIRE(decoded);
    REQUIRE(decoded->type == tunnelpp::MessageType::Error);
    REQUIRE(decoded->id == "id-9");
    REQUIRE(decoded->code == "invalid_token");
    REQUIRE(decoded->detail == "invalid token");
}

TEST_CASE("protocol ping pong round-trip")
{
    tunnelpp::Message ping;
    ping.type = tunnelpp::MessageType::Ping;
    const auto decoded{ tunnelpp::decode_message(tunnelpp::encode_message(ping)) };
    REQUIRE(decoded);
    REQUIRE(decoded->type == tunnelpp::MessageType::Ping);
}

TEST_CASE("decode_message rejects invalid input")
{
    REQUIRE(!tunnelpp::decode_message(""));
    REQUIRE(!tunnelpp::decode_message("not json"));
    REQUIRE(!tunnelpp::decode_message("[1,2]"));
    REQUIRE(!tunnelpp::decode_message(R"({"type":"unknown"})"));
    REQUIRE(!tunnelpp::decode_message(R"({"type":42})"));
    REQUIRE(!tunnelpp::decode_message(R"({"type":"register"})"));
    REQUIRE(!tunnelpp::decode_message(R"({"type":"register","version":"x","subdomain":"a","token":"b"})"));
    REQUIRE(!tunnelpp::decode_message(R"({"type":"request","id":"1","method":"GET","path":"/","headers":{},"body_b64":"!!!"})"));
    REQUIRE(!tunnelpp::decode_message(R"({"type":"response","id":"1","status":200,"headers":"nope"})"));
}
