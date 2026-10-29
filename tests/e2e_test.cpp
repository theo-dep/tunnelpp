#include <catch2/catch_test_macros.hpp>

#include <tunnel_client.h>
#include <tunnel_server.h>

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <subdomain.h>

#include <chrono>
#include <functional>
#include <string>
#include <thread>

using tunnelpp::Message;
using tunnelpp::MessageType;
using tunnelpp::Url;
using tunnelpp::client::ClientConfig;
using tunnelpp::client::TunnelClient;
using tunnelpp::server::TunnelServer;

namespace
{
    class EchoServer
    {
    public:
        EchoServer()
        {
            const auto handler{ [](const httplib::Request& req, httplib::Response& res) {
                const auto body = nlohmann::json{
                    { "method", req.method },
                    { "path", req.target },
                    { "body", req.body },
                    { "x_test", req.get_header_value("X-Test") },
                };
                res.set_content(body.dump(), "application/json");
            } };
            _server.Get(R"((.*))", handler);
            _server.Post(R"((.*))", handler);
            _server.Put(R"((.*))", handler);
            _server.Delete(R"((.*))", handler);
            _port = _server.bind_to_any_port("127.0.0.1");
            _thread = std::thread([this] { _server.listen_after_bind(); });
        }

        ~EchoServer()
        {
            _server.stop();
            _thread.join();
        }

        int port() const
        {
            return _port;
        }

    private:
        httplib::Server _server;
        std::thread _thread;
        int _port{ 0 };
    };

    TunnelServer make_server()
    {
        tunnelpp::ServerConfig config;
        config.domain = "tunnel.test";
        config.token = "secret";
        config.host = "127.0.0.1";
        config.http_port = 0;
        config.admin_port = 0;
        return TunnelServer{ config };
    }

    ClientConfig make_client_config(int server_port, int local_port, const std::string& token, const std::string& subdomain)
    {
        ClientConfig config;
        config.server_url = "ws://127.0.0.1:" + std::to_string(server_port) + "/tunnel";
        config.token = token;
        config.local.scheme = "http";
        config.local.host = "127.0.0.1";
        config.local.port = local_port;
        config.local.path = "/";
        config.subdomain = subdomain;
        return config;
    }

    bool wait_for(const std::function<bool()>& condition, int timeout_seconds)
    {
        for (int i = 0; i < timeout_seconds * 100; ++i) {
            if (condition()) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{ 10 });
        }
        return condition();
    }
}

TEST_CASE("e2e http request through the tunnel")
{
    EchoServer echo;
    auto server{ make_server() };
    REQUIRE(server.start());

    TunnelClient client{ make_client_config(server.public_port(), echo.port(), "secret", "e2e-app") };
    std::thread client_thread{ [&] { client.run(); } };
    REQUIRE(wait_for([&] { return client.is_registered(); }, 5));
    REQUIRE(client.assigned_url() == "http://e2e-app.tunnel.test:" + std::to_string(server.public_port()));

    httplib::Client http{ "127.0.0.1", server.public_port() };
    http.set_read_timeout(10, 0);

    const auto res{ http.Get("/hello?x=1", httplib::Headers{ { "Host", "e2e-app.tunnel.test" }, { "X-Test", "yes" } }) };
    REQUIRE(res);
    REQUIRE(res->status == 200);
    const auto body = nlohmann::json::parse(res->body);
    REQUIRE(body.at("method") == "GET");
    REQUIRE(body.at("path") == "/hello?x=1");
    REQUIRE(body.at("x_test") == "yes");

    const auto post{ http.Post("/api", httplib::Headers{ { "Host", "e2e-app.tunnel.test" } }, "payload", "text/plain") };
    REQUIRE(post);
    REQUIRE(post->status == 200);
    REQUIRE(nlohmann::json::parse(post->body).at("body") == "payload");

    const auto not_found{ http.Get("/", httplib::Headers{ { "Host", "nope.tunnel.test" } }) };
    REQUIRE(not_found);
    REQUIRE(not_found->status == 404);

    const auto wrong_host{ http.Get("/", httplib::Headers{ { "Host", "example.com" } }) };
    REQUIRE(wrong_host);
    REQUIRE(wrong_host->status == 404);

    client.stop();
    client_thread.join();
    server.stop();
}

TEST_CASE("e2e admin api")
{
    EchoServer echo;
    auto server{ make_server() };
    REQUIRE(server.start());

    TunnelClient client{ make_client_config(server.public_port(), echo.port(), "secret", "e2e-app") };
    std::thread client_thread{ [&] { client.run(); } };
    REQUIRE(wait_for([&] { return client.is_registered(); }, 5));

    httplib::Client admin{ "127.0.0.1", server.admin_port() };
    admin.set_read_timeout(5, 0);

    const auto health{ admin.Get("/healthz") };
    REQUIRE(health);
    REQUIRE(health->status == 200);
    REQUIRE(nlohmann::json::parse(health->body).at("status") == "ok");

    const auto sessions{ admin.Get("/api/v1/sessions") };
    REQUIRE(sessions);
    REQUIRE(sessions->status == 200);
    const auto sessions_body = nlohmann::json::parse(sessions->body);
    REQUIRE(sessions_body.at("sessions").size() == 1);
    REQUIRE(sessions_body.at("sessions").at(0).at("subdomain") == "e2e-app");

    httplib::Client http{ "127.0.0.1", server.public_port() };
    http.set_read_timeout(10, 0);
    REQUIRE(http.Get("/", httplib::Headers{ { "Host", "e2e-app.tunnel.test" } }));

    const auto stats{ admin.Get("/api/v1/stats") };
    REQUIRE(stats);
    REQUIRE(stats->status == 200);
    const auto stats_body = nlohmann::json::parse(stats->body);
    REQUIRE(stats_body.at("active_sessions") == 1);
    REQUIRE(stats_body.at("total_requests") >= 1);

    client.stop();
    client_thread.join();
    server.stop();
}

TEST_CASE("e2e wrong token is rejected")
{
    EchoServer echo;
    auto server{ make_server() };
    REQUIRE(server.start());

    TunnelClient client{ make_client_config(server.public_port(), echo.port(), "wrong", "e2e-app") };
    std::thread client_thread{ [&] { client.run(); } };
    REQUIRE(wait_for([&] { return client.fatal(); }, 5));
    REQUIRE(!client.is_registered());
    client_thread.join();
    REQUIRE(server.stats().rejected_registers >= 1);
    server.stop();
}

TEST_CASE("e2e subdomain conflict is rejected")
{
    EchoServer echo;
    auto server{ make_server() };
    REQUIRE(server.start());

    TunnelClient first{ make_client_config(server.public_port(), echo.port(), "secret", "e2e-app") };
    std::thread first_thread{ [&] { first.run(); } };
    REQUIRE(wait_for([&] { return first.is_registered(); }, 5));

    TunnelClient second{ make_client_config(server.public_port(), echo.port(), "secret", "e2e-app") };
    std::thread second_thread{ [&] { second.run(); } };
    REQUIRE(wait_for([&] { return second.fatal(); }, 5));
    REQUIRE(!second.is_registered());

    first.stop();
    first_thread.join();
    second_thread.join();
    server.stop();
}

TEST_CASE("e2e random subdomain")
{
    EchoServer echo;
    auto server{ make_server() };
    REQUIRE(server.start());

    TunnelClient client{ make_client_config(server.public_port(), echo.port(), "secret", "") };
    std::thread client_thread{ [&] { client.run(); } };
    REQUIRE(wait_for([&] { return client.is_registered(); }, 5));
    const auto url{ client.assigned_url() };
    const auto suffix{ ".tunnel.test:" + std::to_string(server.public_port()) };
    REQUIRE(url.starts_with("http://"));
    REQUIRE(url.ends_with(suffix));
    const auto subdomain{ url.substr(7, url.size() - 7 - suffix.size()) };
    REQUIRE(tunnelpp::is_valid_subdomain(subdomain));

    client.stop();
    client_thread.join();
    server.stop();
}
