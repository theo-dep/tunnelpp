#include "pending.h"

#include <chrono>
#include <vector>

namespace tunnelpp::server
{
    std::shared_ptr<PendingRequest> PendingRequestTable::add(const std::string& id, httplib::ws::WebSocket* owner)
    {
        auto request{ std::make_shared<PendingRequest>() };
        request->id = id;
        request->owner = owner;
        std::scoped_lock lock{ _mutex };
        _entries.emplace(id, request);
        return request;
    }

    void PendingRequestTable::remove(const std::string& id)
    {
        std::shared_ptr<PendingRequest> request;
        {
            std::scoped_lock lock{ _mutex };
            const auto it{ _entries.find(id) };
            if (it == _entries.end()) {
                return;
            }
            request = it->second;
            _entries.erase(it);
        }
        {
            std::scoped_lock lock{ request->mutex };
            request->failed = true;
        }
        request->cv.notify_all();
    }

    void PendingRequestTable::complete(const std::string& id, const Message& response)
    {
        std::shared_ptr<PendingRequest> request;
        {
            std::scoped_lock lock{ _mutex };
            const auto it{ _entries.find(id) };
            if (it == _entries.end()) {
                return;
            }
            request = it->second;
            _entries.erase(it);
        }
        {
            std::scoped_lock lock{ request->mutex };
            request->response = response;
            request->completed = true;
        }
        request->cv.notify_all();
    }

    void PendingRequestTable::fail_all_for(httplib::ws::WebSocket* owner)
    {
        std::vector<std::shared_ptr<PendingRequest>> requests;
        {
            std::scoped_lock lock{ _mutex };
            for (auto it = _entries.begin(); it != _entries.end();) {
                if (it->second->owner == owner) {
                    requests.push_back(it->second);
                    it = _entries.erase(it);
                } else {
                    ++it;
                }
            }
        }
        for (const auto& request : requests) {
            {
                std::scoped_lock lock{ request->mutex };
                request->failed = true;
            }
            request->cv.notify_all();
        }
    }

    PendingRequestTable::WaitOutcome PendingRequestTable::wait(const std::shared_ptr<PendingRequest>& request, int timeout_seconds, Message& out)
    {
        std::unique_lock lock{ request->mutex };
        const auto done{ request->cv.wait_for(lock, std::chrono::seconds{ timeout_seconds }, [&] { return request->completed || request->failed; }) };
        if (!done) {
            lock.unlock();
            remove(request->id);
            return WaitOutcome::Timeout;
        }
        if (request->failed) {
            return WaitOutcome::Failed;
        }
        out = request->response;
        return WaitOutcome::Ok;
    }

    std::size_t PendingRequestTable::size() const
    {
        std::scoped_lock lock{ _mutex };
        return _entries.size();
    }
}
