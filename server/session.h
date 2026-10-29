#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace httplib::ws
{
    class WebSocket;
}

namespace tunnelpp::server
{
    class Session
    {
    public:
        Session(std::string subdomain, std::string url, httplib::ws::WebSocket* ws);

        const std::string& subdomain() const;
        const std::string& url() const;
        httplib::ws::WebSocket* ws() const;
        std::chrono::system_clock::time_point connected_at() const;

        bool send(const std::string& data);
        void close();
        bool is_open() const;

        std::atomic<uint64_t>& requests();
        std::atomic<uint64_t>& bytes_in();
        std::atomic<uint64_t>& bytes_out();

    private:
        std::string _subdomain;
        std::string _url;
        httplib::ws::WebSocket* _ws;
        std::chrono::system_clock::time_point _connected_at;
        std::atomic<uint64_t> _requests{ 0 };
        std::atomic<uint64_t> _bytes_in{ 0 };
        std::atomic<uint64_t> _bytes_out{ 0 };
    };

    enum class RegisterResult
    {
        Ok,
        InvalidSubdomain,
        SubdomainTaken,
        AlreadyRegistered,
    };

    class SessionManager
    {
    public:
        RegisterResult register_session(const std::string& subdomain, const std::string& url, httplib::ws::WebSocket* ws, std::shared_ptr<Session>& out);
        void unregister(httplib::ws::WebSocket* ws);
        std::shared_ptr<Session> find(const std::string& subdomain) const;
        std::vector<std::shared_ptr<Session>> snapshot() const;
        std::size_t size() const;

    private:
        mutable std::mutex _mutex;
        std::map<std::string, std::shared_ptr<Session>> _by_subdomain;
        std::unordered_map<httplib::ws::WebSocket*, std::shared_ptr<Session>> _by_ws;
    };
}
