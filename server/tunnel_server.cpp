#include "tunnel_server.h"

#include "router.h"

#include <headers.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <protocol.h>
#include <spdlog/spdlog.h>
#include <stringutils.h>
#include <uuid.h>

#include <chrono>

namespace tunnelpp::server
{
    static constexpr int status_ok{ 200 };
    static constexpr int status_not_found{ 404 };
    static constexpr int status_bad_gateway{ 502 };
    static constexpr int status_gateway_timeout{ 504 };
    static constexpr int ping_interval_seconds{ 15 };
    static constexpr int ws_read_timeout_seconds{ 10 };
    static constexpr int https_port{ 443 };
    static constexpr int http_port{ 80 };

    static void set_json(httplib::Response& res, int status, const std::string& body)
    {
        res.status = status;
        res.set_content(body, "application/json");
    }

    static bool bind_listener(httplib::Server& server, const std::string& host, int port, int& bound_port)
    {
        if (port == 0) {
            bound_port = server.bind_to_any_port(host);
            return bound_port > 0;
        }
        if (!server.bind_to_port(host, port)) {
            return false;
        }
        bound_port = port;
        return true;
    }

    TunnelServer::TunnelServer(ServerConfig config)
        : _config{ std::move(config) }
        , _public_server{ std::make_unique<httplib::Server>() }
    {
        _public_server->set_websocket_ping_interval(ping_interval_seconds);
        _public_server->set_websocket_max_missed_pongs(3);
        // clang-tidy 21: false-positive NewDeleteLeaks reported inside httplib.h from this call
        _public_server->WebSocket("/", [this](const httplib::Request&, httplib::ws::WebSocket& ws) { // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
            handle_ws(ws);
        });
        const auto catch_all{ [this](const httplib::Request& req, httplib::Response& res) {
            handle_public_request(req, res);
        } };
        _public_server->Get(R"((.*))", catch_all);
        _public_server->Post(R"((.*))", catch_all);
        _public_server->Put(R"((.*))", catch_all);
        _public_server->Patch(R"((.*))", catch_all);
        _public_server->Delete(R"((.*))", catch_all);
        _public_server->Options(R"((.*))", catch_all);
    }

    TunnelServer::~TunnelServer()
    {
        stop();
    }

    bool TunnelServer::start()
    {
        if (!bind_listener(*_public_server, _config.host, _config.http_port, _public_port)) {
            spdlog::error("failed to bind public listener on {}:{}", _config.host, _config.http_port);
            return false;
        }
        _running = true;
        _public_thread = std::thread([this] { _public_server->listen_after_bind(); });
        return true;
    }

    void TunnelServer::stop()
    {
        if (!_running.exchange(false)) {
            return;
        }
        for (const auto& session : _sessions.snapshot()) {
            session->close();
        }
        _public_server->stop();
        if (_public_thread.joinable()) {
            _public_thread.join();
        }
    }

    bool TunnelServer::is_running() const
    {
        return _running;
    }

    int TunnelServer::public_port() const
    {
        return _public_port;
    }

    const ServerConfig& TunnelServer::config() const
    {
        return _config;
    }

    SessionManager& TunnelServer::sessions()
    {
        return _sessions;
    }

    std::string TunnelServer::public_url(const std::string& subdomain) const
    {
        std::string url{ _config.public_scheme + "://" + subdomain + "." + _config.domain };
        const int default_port{ _config.public_scheme == "https" ? https_port : http_port };
        const int port{ _config.public_port > 0 ? _config.public_port : _public_port };
        if (port != default_port) {
            url += ":" + std::to_string(port);
        }
        return url;
    }

    void TunnelServer::handle_public_request(const httplib::Request& req, httplib::Response& res)
    {
        if (req.method == "GET" && req.path == "/" && host_without_port(req.get_header_value("Host")) == _config.domain) {
            set_json(res, status_ok, nlohmann::json{ { "name", "tunnelpp" }, { "version", TUNNELPP_VERSION } }.dump());
            return;
        }

        const auto subdomain{ extract_subdomain(req.get_header_value("Host"), _config.domain) };
        const auto session{ subdomain ? _sessions.find(*subdomain) : nullptr };
        if (!session || !session->is_open()) {
            set_json(res, status_not_found, R"({"error":"tunnel_not_found","message":"no tunnel for this host"})");
            return;
        }

        Message message;
        message.type = MessageType::Request;
        message.id = make_uuid();
        message.method = req.method;
        message.path = req.target;
        message.headers = filter_tunnel_headers(req.headers);
        message.body = req.body;

        const auto pending{ _pending.add(message.id, session->ws()) };
        if (!session->send(encode_message(message))) {
            _pending.remove(message.id);
            set_json(res, status_bad_gateway, R"({"error":"bad_gateway","message":"tunnel is not connected"})");
            return;
        }

        Message response;
        switch (_pending.wait(pending, _config.request_timeout_seconds, response)) {
            case PendingRequestTable::WaitOutcome::Timeout:
                set_json(res, status_gateway_timeout, R"({"error":"gateway_timeout","message":"the local service did not answer in time"})");
                return;
            case PendingRequestTable::WaitOutcome::Failed:
                set_json(res, status_bad_gateway, R"({"error":"bad_gateway","message":"tunnel disconnected"})");
                return;
            case PendingRequestTable::WaitOutcome::Ok:
                break;
        }

        res.status = response.status;
        std::string content_type{ "application/octet-stream" };
        for (const auto& [key, value] : response.headers) {
            if (lower(key) == "content-type") {
                content_type = value;
            } else {
                res.set_header(key, value);
            }
        }
        res.set_content(response.body, content_type);
    }

    void TunnelServer::handle_ws(httplib::ws::WebSocket& ws)
    {
        ws.set_read_timeout(std::chrono::seconds{ ws_read_timeout_seconds });
        std::string raw;
        if (ws.read(raw) != httplib::ws::Text) {
            ws.close();
            return;
        }
        const auto message{ decode_message(raw) };
        if (!message || message->type != MessageType::Register) {
            send_ws_error(ws, "", "protocol_error", "first message must be register");
            ws.close();
            return;
        }
        if (message->version != protocol_version) {
            send_ws_error(ws, "", "unsupported_version", "unsupported protocol version");
            ws.close();
            return;
        }
        if (message->token != _config.token) {
            send_ws_error(ws, "", "invalid_token", "invalid token");
            ws.close();
            return;
        }
        std::shared_ptr<Session> session;
        switch (_sessions.register_session(message->subdomain, public_url(message->subdomain), &ws, session)) {
            case RegisterResult::InvalidSubdomain:
                send_ws_error(ws, "", "invalid_subdomain", "invalid subdomain (3-63 lowercase letters, digits or hyphens)");
                ws.close();
                return;
            case RegisterResult::SubdomainTaken:
                send_ws_error(ws, "", "subdomain_taken", "subdomain already in use");
                ws.close();
                return;
            case RegisterResult::AlreadyRegistered:
                send_ws_error(ws, "", "protocol_error", "already registered");
                ws.close();
                return;
            case RegisterResult::Ok:
                break;
        }

        Message registered;
        registered.type = MessageType::Registered;
        registered.subdomain = session->subdomain();
        registered.url = session->url();
        if (!ws.send(encode_message(registered))) {
            _sessions.unregister(&ws);
            ws.close();
            return;
        }
        spdlog::info("tunnel registered: {}", session->url());

        ws.set_read_timeout(std::chrono::seconds{ 0 });
        for (;;) {
            std::string data;
            const auto result{ ws.read(data) };
            if (result == httplib::ws::Fail) {
                break;
            }
            if (result == httplib::ws::Timeout || result == httplib::ws::Binary) {
                continue;
            }
            const auto incoming{ decode_message(data) };
            if (!incoming) {
                send_ws_error(ws, "", "bad_message", "invalid message");
                continue;
            }
            handle_ws_message(*incoming, ws);
        }

        _pending.fail_all_for(&ws);
        _sessions.unregister(&ws);
        spdlog::info("tunnel disconnected: {}", session->subdomain());
        ws.close();
    }

    void TunnelServer::handle_ws_message(const Message& message, httplib::ws::WebSocket& ws)
    {
        switch (message.type) {
            case MessageType::Response:
                _pending.complete(message.id, message);
                break;
            case MessageType::Ping: {
                Message pong;
                pong.type = MessageType::Pong;
                ws.send(encode_message(pong));
                break;
            }
            case MessageType::Pong:
                break;
            case MessageType::Error:
                spdlog::warn("client error on a tunnel: {} ({})", message.code, message.detail);
                break;
            default:
                send_ws_error(ws, message.id, "protocol_error", "unexpected message type");
                break;
        }
    }

    void TunnelServer::send_ws_error(httplib::ws::WebSocket& ws, const std::string& id, const std::string& code, const std::string& detail)
    {
        Message error;
        error.type = MessageType::Error;
        error.id = id;
        error.code = code;
        error.detail = detail;
        ws.send(encode_message(error));
    }
}
