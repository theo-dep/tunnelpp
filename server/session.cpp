#include "session.h"

#include <subdomain.h>

#include <httplib.h>

namespace tunnelpp::server
{
    Session::Session(std::string subdomain, std::string url, httplib::ws::WebSocket* ws)
        : _subdomain{ std::move(subdomain) }
        , _url{ std::move(url) }
        , _ws{ ws }
        , _connected_at{ std::chrono::system_clock::now() }
    {
    }

    const std::string& Session::subdomain() const
    {
        return _subdomain;
    }

    const std::string& Session::url() const
    {
        return _url;
    }

    httplib::ws::WebSocket* Session::ws() const
    {
        return _ws;
    }

    std::chrono::system_clock::time_point Session::connected_at() const
    {
        return _connected_at;
    }

    bool Session::send(const std::string& data)
    {
        return _ws != nullptr && _ws->send(data);
    }

    void Session::close()
    {
        if (_ws != nullptr) {
            _ws->close();
        }
    }

    bool Session::is_open() const
    {
        return _ws != nullptr && _ws->is_open();
    }

    std::atomic<uint64_t>& Session::requests()
    {
        return _requests;
    }

    std::atomic<uint64_t>& Session::bytes_in()
    {
        return _bytes_in;
    }

    std::atomic<uint64_t>& Session::bytes_out()
    {
        return _bytes_out;
    }

    RegisterResult SessionManager::register_session(const std::string& subdomain, const std::string& url, httplib::ws::WebSocket* ws, std::shared_ptr<Session>& out)
    {
        std::scoped_lock lock{ _mutex };
        if (!is_valid_subdomain(subdomain)) {
            return RegisterResult::InvalidSubdomain;
        }
        if (ws != nullptr && _by_ws.contains(ws)) {
            return RegisterResult::AlreadyRegistered;
        }
        if (_by_subdomain.contains(subdomain)) {
            return RegisterResult::SubdomainTaken;
        }
        out = std::make_shared<Session>(subdomain, url, ws);
        _by_subdomain.emplace(subdomain, out);
        _by_ws.emplace(ws, out);
        return RegisterResult::Ok;
    }

    void SessionManager::unregister(httplib::ws::WebSocket* ws)
    {
        std::scoped_lock lock{ _mutex };
        const auto it{ _by_ws.find(ws) };
        if (it == _by_ws.end()) {
            return;
        }
        _by_subdomain.erase(it->second->subdomain());
        _by_ws.erase(it);
    }

    std::shared_ptr<Session> SessionManager::find(const std::string& subdomain) const
    {
        std::scoped_lock lock{ _mutex };
        const auto it{ _by_subdomain.find(subdomain) };
        return it == _by_subdomain.end() ? nullptr : it->second;
    }

    std::vector<std::shared_ptr<Session>> SessionManager::snapshot() const
    {
        std::scoped_lock lock{ _mutex };
        std::vector<std::shared_ptr<Session>> result;
        result.reserve(_by_subdomain.size());
        for (const auto& [subdomain, session] : _by_subdomain) {
            result.push_back(session);
        }
        return result;
    }

    std::size_t SessionManager::size() const
    {
        std::scoped_lock lock{ _mutex };
        return _by_subdomain.size();
    }
}
