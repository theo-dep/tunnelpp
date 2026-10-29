#include "tunnel_client.h"

#include "local_forwarder.h"

#include <protocol.h>
#include <spdlog/spdlog.h>
#include <subdomain.h>

#include <httplib.h>

#include <algorithm>
#include <chrono>
#include <thread>

namespace tunnelpp::client
{
    static constexpr int connect_timeout_seconds{ 10 };
    static constexpr int ping_interval_seconds{ 15 };
    static constexpr int worker_poll_ms{ 10 };
    static constexpr int sleep_poll_ms{ 100 };
    static constexpr int polls_per_second{ 10 };

    TunnelClient::TunnelClient(ClientConfig config)
        : _config{ std::move(config) }
        , _rng{ std::random_device{}() }
    {
    }

    TunnelClient::~TunnelClient()
    {
        stop();
        while (_active_workers > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds{ worker_poll_ms });
        }
    }

    void TunnelClient::run()
    {
        _running = true;
        int delay_seconds{ 1 };
        while (_running) {
            const std::string subdomain{ _config.subdomain.empty() ? random_subdomain(_rng) : _config.subdomain };
            const bool was_registered{ run_session(subdomain) };
            if (_fatal || !_running) {
                break;
            }
            if (was_registered) {
                delay_seconds = 1;
            }
            spdlog::warn("reconnecting to {} in {}s", _config.server_url, delay_seconds);
            sleep_interruptible(delay_seconds);
            delay_seconds = std::min(delay_seconds * 2, _config.max_reconnect_delay_seconds);
        }
        _running = false;
    }

    void TunnelClient::stop()
    {
        _running = false;
        std::shared_ptr<httplib::ws::WebSocketClient> ws;
        {
            std::scoped_lock lock{ _ws_mutex };
            ws = _current_ws;
        }
        if (ws) {
            ws->close();
        }
    }

    bool TunnelClient::is_registered() const
    {
        return _registered;
    }

    bool TunnelClient::fatal() const
    {
        return _fatal;
    }

    std::string TunnelClient::assigned_url() const
    {
        std::scoped_lock lock{ _state_mutex };
        return _assigned_url;
    }

    bool TunnelClient::run_session(const std::string& subdomain)
    {
        auto ws{ std::make_shared<httplib::ws::WebSocketClient>(_config.server_url) };
        {
            std::scoped_lock lock{ _ws_mutex };
            _current_ws = ws;
        }
        ws->set_connection_timeout(connect_timeout_seconds, 0);
        ws->set_websocket_ping_interval(ping_interval_seconds);
        ws->set_websocket_max_missed_pongs(3);

        if (!ws->connect()) {
            spdlog::warn("failed to connect to {}", _config.server_url);
            std::scoped_lock lock{ _ws_mutex };
            if (_current_ws == ws) {
                _current_ws.reset();
            }
            return false;
        }
        spdlog::info("connected to {}", _config.server_url);

        Message register_message;
        register_message.type = MessageType::Register;
        register_message.version = protocol_version;
        register_message.subdomain = subdomain;
        register_message.token = _config.token;
        if (!ws->send(encode_message(register_message))) {
            spdlog::warn("failed to send register to {}", _config.server_url);
            std::scoped_lock lock{ _ws_mutex };
            if (_current_ws == ws) {
                _current_ws.reset();
            }
            return false;
        }

        bool was_registered{ false };
        while (_running) {
            std::string raw;
            const auto result{ ws->read(raw) };
            if (result == httplib::ws::Fail) {
                break;
            }
            if (result == httplib::ws::Timeout || result == httplib::ws::Binary) {
                continue;
            }
            const auto message{ decode_message(raw) };
            if (!message) {
                spdlog::warn("invalid message from server");
                continue;
            }
            handle_session_message(*message, ws, register_message, was_registered);
        }

        clear_registered();
        spdlog::warn("disconnected from {}", _config.server_url);
        {
            std::scoped_lock lock{ _ws_mutex };
            if (_current_ws == ws) {
                _current_ws.reset();
            }
        }
        ws->close();
        return was_registered;
    }

    void TunnelClient::handle_session_message(const Message& message, const std::shared_ptr<httplib::ws::WebSocketClient>& ws, Message& register_message, bool& was_registered)
    {
        switch (message.type) {
            case MessageType::Registered:
                was_registered = true;
                set_registered(message.url);
                spdlog::info("tunnel ready: {} -> {}:{}", message.url, _config.local.host, _config.local.port_or_default());
                break;
            case MessageType::Request: {
                auto request{ message };
                std::thread([this, ws, request = std::move(request)]() mutable {
                    forward_and_send(ws, request);
                }).detach();
                break;
            }
            case MessageType::Ping: {
                Message pong;
                pong.type = MessageType::Pong;
                ws->send(encode_message(pong));
                break;
            }
            case MessageType::Pong:
                break;
            case MessageType::Error:
                spdlog::error("server error: {} ({})", message.code, message.detail);
                if (message.code == "invalid_token" || message.code == "invalid_subdomain" || message.code == "unsupported_version") {
                    _fatal = true;
                } else if (message.code == "subdomain_taken") {
                    if (_config.subdomain.empty()) {
                        register_message.subdomain = random_subdomain(_rng);
                        ws->send(encode_message(register_message));
                    } else {
                        _fatal = true;
                    }
                }
                break;
            default:
                break;
        }
    }

    void TunnelClient::forward_and_send(const std::shared_ptr<httplib::ws::WebSocketClient>& ws, const Message& request)
    {
        _active_workers++;
        const auto response{ forward_to_local(_config.local, request, 30) };
        ws->send(encode_message(response));
        _active_workers--;
    }

    void TunnelClient::set_registered(const std::string& url)
    {
        {
            std::scoped_lock lock{ _state_mutex };
            _assigned_url = url;
        }
        _registered = true;
    }

    void TunnelClient::clear_registered()
    {
        {
            std::scoped_lock lock{ _state_mutex };
            _assigned_url.clear();
        }
        _registered = false;
    }

    void TunnelClient::sleep_interruptible(int seconds)
    {
        for (int i = 0; i < seconds * polls_per_second && _running; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds{ sleep_poll_ms });
        }
    }
}
