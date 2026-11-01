#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include <config.h>

#include "pending.h"
#include "session.h"

namespace httplib
{
    class Server;
    class Request;
    class Response;
    namespace ws
    {
        class WebSocket;
    }
}

namespace tunnelpp::server
{
    class TunnelServer
    {
    public:
        explicit TunnelServer(ServerConfig config);
        ~TunnelServer();

        bool start();
        void stop();
        bool is_running() const;
        int public_port() const;

        const ServerConfig& config() const;
        SessionManager& sessions();

    private:
        void handle_public_request(const httplib::Request& req, httplib::Response& res);
        void handle_ws(httplib::ws::WebSocket& ws);
        void handle_ws_message(const Message& message, httplib::ws::WebSocket& ws);
        static void send_ws_error(httplib::ws::WebSocket& ws, const std::string& id, const std::string& code, const std::string& detail);
        std::string public_url(const std::string& subdomain) const;

        ServerConfig _config;
        std::unique_ptr<httplib::Server> _public_server;
        std::thread _public_thread;
        SessionManager _sessions;
        PendingRequestTable _pending;
        int _public_port{ 0 };
        std::atomic<bool> _running{ false };
    };
}
