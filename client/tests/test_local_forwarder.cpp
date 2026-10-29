#include <catch2/catch_test_macros.hpp>

#include <local_forwarder.h>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <thread>

using tunnelpp::Message;
using tunnelpp::MessageType;
using tunnelpp::Url;
using tunnelpp::client::forward_to_local;

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

    Url local_url(int port)
    {
        Url url;
        url.scheme = "http";
        url.host = "127.0.0.1";
        url.port = port;
        url.path = "/";
        return url;
    }

    Message make_request(const std::string& method, const std::string& path, const std::string& body = "")
    {
        Message request;
        request.type = MessageType::Request;
        request.id = "id-1";
        request.method = method;
        request.path = path;
        request.body = body;
        return request;
    }
}

TEST_CASE("forward_to_local get")
{
    EchoServer echo;
    auto request{ make_request("GET", "/hello?x=1") };
    request.headers.emplace("X-Test", "yes");
    const auto response{ forward_to_local(local_url(echo.port()), request, 5) };
    REQUIRE(response.type == MessageType::Response);
    REQUIRE(response.id == "id-1");
    REQUIRE(response.status == 200);
    const auto body = nlohmann::json::parse(response.body);
    REQUIRE(body.at("method") == "GET");
    REQUIRE(body.at("path") == "/hello?x=1");
    REQUIRE(body.at("x_test") == "yes");
}

TEST_CASE("forward_to_local post with body")
{
    EchoServer echo;
    const auto response{ forward_to_local(local_url(echo.port()), make_request("POST", "/api", "hello body"), 5) };
    REQUIRE(response.status == 200);
    const auto body = nlohmann::json::parse(response.body);
    REQUIRE(body.at("method") == "POST");
    REQUIRE(body.at("body") == "hello body");
}

TEST_CASE("forward_to_local connection refused")
{
    httplib::Server probe;
    const int free_port{ probe.bind_to_any_port("127.0.0.1") };
    probe.stop();
    const auto response{ forward_to_local(local_url(free_port), make_request("GET", "/"), 2) };
    REQUIRE(response.status == 502);
    REQUIRE(response.headers.count("Content-Type") == 1);
}
