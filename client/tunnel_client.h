#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <random>
#include <string>

#include <url.h>

namespace httplib::ws
{
    class WebSocketClient;
}

namespace tunnelpp
{
    struct Message;
}

namespace tunnelpp::client
{
    struct ClientConfig
    {
        std::string server_url;
        std::string token;
        Url local;
        std::string subdomain; // empty = random
        int max_reconnect_delay_seconds{ 30 };
    };

    class TunnelClient
    {
    public:
        explicit TunnelClient(ClientConfig config);
        ~TunnelClient();

        void run(); // blocking, reconnects until stop() or a fatal error
        void stop();
        bool is_registered() const;
        bool fatal() const;
        std::string assigned_url() const;

    private:
        bool run_session(const std::string& subdomain); // true when the session reached the registered state
        void handle_session_message(const Message& message, const std::shared_ptr<httplib::ws::WebSocketClient>& ws, Message& register_message, bool& was_registered);
        void forward_and_send(const std::shared_ptr<httplib::ws::WebSocketClient>& ws, const Message& request);
        void set_registered(const std::string& url);
        void clear_registered();
        void sleep_interruptible(int seconds);

        ClientConfig _config;
        std::atomic<bool> _running{ false };
        std::atomic<bool> _registered{ false };
        std::atomic<bool> _fatal{ false };
        std::atomic<int> _active_workers{ 0 };
        std::mutex _ws_mutex;
        std::shared_ptr<httplib::ws::WebSocketClient> _current_ws;
        mutable std::mutex _state_mutex;
        std::string _assigned_url;
        std::mt19937 _rng;
    };
}
