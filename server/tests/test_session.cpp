#include <catch2/catch_test_macros.hpp>

#include <session.h>

using tunnelpp::server::RegisterResult;
using tunnelpp::server::Session;
using tunnelpp::server::SessionManager;

TEST_CASE("SessionManager register and find")
{
    SessionManager manager;
    std::shared_ptr<Session> session;
    REQUIRE(manager.register_session("my-app", "http://my-app.tunnel.test", nullptr, session) == RegisterResult::Ok);
    REQUIRE(session);
    REQUIRE(session->subdomain() == "my-app");
    REQUIRE(session->url() == "http://my-app.tunnel.test");
    REQUIRE(!session->is_open());
    REQUIRE(manager.find("my-app") != nullptr);
    REQUIRE(manager.find("other") == nullptr);
    REQUIRE(manager.size() == 1);
}

TEST_CASE("SessionManager rejects invalid subdomain")
{
    SessionManager manager;
    std::shared_ptr<Session> session;
    REQUIRE(manager.register_session("AB", "http://ab.tunnel.test", nullptr, session) == RegisterResult::InvalidSubdomain);
    REQUIRE(manager.size() == 0);
}

TEST_CASE("SessionManager rejects taken subdomain")
{
    SessionManager manager;
    std::shared_ptr<Session> first;
    std::shared_ptr<Session> second;
    int ws_a{ 0 };
    int ws_b{ 0 };
    auto* a{ reinterpret_cast<httplib::ws::WebSocket*>(&ws_a) };
    auto* b{ reinterpret_cast<httplib::ws::WebSocket*>(&ws_b) };
    REQUIRE(manager.register_session("my-app", "http://my-app.tunnel.test", a, first) == RegisterResult::Ok);
    REQUIRE(manager.register_session("my-app", "http://my-app.tunnel.test", b, second) == RegisterResult::SubdomainTaken);
    REQUIRE(manager.size() == 1);
}

TEST_CASE("SessionManager rejects double registration on the same socket")
{
    SessionManager manager;
    std::shared_ptr<Session> session;
    int ws{ 0 };
    auto* socket{ reinterpret_cast<httplib::ws::WebSocket*>(&ws) };
    REQUIRE(manager.register_session("my-app", "http://my-app.tunnel.test", socket, session) == RegisterResult::Ok);
    REQUIRE(manager.register_session("other-app", "http://other-app.tunnel.test", socket, session) == RegisterResult::AlreadyRegistered);
    REQUIRE(manager.size() == 1);
}

TEST_CASE("SessionManager unregister frees the subdomain")
{
    SessionManager manager;
    std::shared_ptr<Session> session;
    int ws{ 0 };
    auto* socket{ reinterpret_cast<httplib::ws::WebSocket*>(&ws) };
    REQUIRE(manager.register_session("my-app", "http://my-app.tunnel.test", socket, session) == RegisterResult::Ok);
    manager.unregister(socket);
    REQUIRE(manager.find("my-app") == nullptr);
    REQUIRE(manager.size() == 0);
    REQUIRE(manager.register_session("my-app", "http://my-app.tunnel.test", socket, session) == RegisterResult::Ok);
    REQUIRE(manager.size() == 1);
}

TEST_CASE("SessionManager snapshot")
{
    SessionManager manager;
    std::shared_ptr<Session> session;
    REQUIRE(manager.register_session("app-a", "http://app-a.tunnel.test", nullptr, session) == RegisterResult::Ok);
    REQUIRE(manager.register_session("app-b", "http://app-b.tunnel.test", nullptr, session) == RegisterResult::Ok);
    const auto snapshot{ manager.snapshot() };
    REQUIRE(snapshot.size() == 2);
    REQUIRE(snapshot[0]->subdomain() == "app-a");
    REQUIRE(snapshot[1]->subdomain() == "app-b");
}
