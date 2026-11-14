#include "local_forwarder.h"

#include <headers.h>

#include <httplib.h>
#include <spdlog/spdlog.h>

#include <string>

namespace tunnelpp::client
{
    Message forward_to_local(const Url& local, const Message& request, int timeout_seconds)
    {
        constexpr int connect_timeout_seconds{ 10 };
        constexpr int write_timeout_seconds{ 30 };
        constexpr int status_bad_gateway{ 502 };
        Message response;
        response.type = MessageType::Response;
        response.id = request.id;

        const auto host{ local.host.contains(':') ? "[" + local.host + "]" : local.host };
        httplib::Client client{ local.scheme + "://" + host + ":" + std::to_string(local.port_or_default()) };
        // request.path is the raw req.target from the tunnel server (already percent-encoded by
        // the browser): send it as-is, httplib's default re-encoding would turn '+' into '%2B'
        client.set_path_encode(false);
        if (local.scheme == "https") {
            // local services usually serve https with a self-signed certificate
            client.enable_server_certificate_verification(false);
            client.enable_server_hostname_verification(false);
        }
        client.set_connection_timeout(connect_timeout_seconds, 0);
        client.set_read_timeout(timeout_seconds, 0);
        client.set_write_timeout(write_timeout_seconds, 0);

        httplib::Request forward_request;
        forward_request.method = request.method;
        forward_request.path = request.path;
        forward_request.headers = request.headers;
        forward_request.body = request.body;

        const auto result{ client.send(forward_request) };
        if (!result) {
            spdlog::warn("forward to {}:{} failed: {}", host, local.port_or_default(), httplib::to_string(result.error()));
            response.status = status_bad_gateway;
            response.headers.emplace("Content-Type", "application/json");
            response.body = R"({"error":"bad_gateway","message":"failed to reach local service"})";
            return response;
        }
        spdlog::debug("forward {} {} -> {}", request.method, request.path, result->status);
        response.status = result->status;
        response.headers = filter_tunnel_headers(result->headers);
        response.body = result->body;
        return response;
    }
}
