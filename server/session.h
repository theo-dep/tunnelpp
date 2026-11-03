#pragma once

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

        bool send(const std::string& data);
        void close();
        bool is_open() const;

    private:
        std::string _subdomain;
        std::string _url;
        httplib::ws::WebSocket* _ws;
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
