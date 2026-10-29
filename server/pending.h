#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <protocol.h>

namespace httplib::ws
{
    class WebSocket;
}

namespace tunnelpp::server
{
    struct PendingRequest
    {
        std::string id;
        httplib::ws::WebSocket* owner;
        std::mutex mutex;
        std::condition_variable cv;
        bool completed{ false };
        bool failed{ false };
        Message response;
    };

    class PendingRequestTable
    {
    public:
        enum class WaitOutcome
        {
            Ok,
            Timeout,
            Failed,
        };

        std::shared_ptr<PendingRequest> add(const std::string& id, httplib::ws::WebSocket* owner);
        void remove(const std::string& id);
        void complete(const std::string& id, const Message& response);
        void fail_all_for(httplib::ws::WebSocket* owner);
        WaitOutcome wait(const std::shared_ptr<PendingRequest>& request, int timeout_seconds, Message& out);
        std::size_t size() const;

    private:
        mutable std::mutex _mutex;
        std::unordered_map<std::string, std::shared_ptr<PendingRequest>> _entries;
    };
}
