#include "args.h"

#include <argparse/argparse.hpp>
#include <subdomain.h>

#include <iostream>

namespace tunnelpp::client
{
    std::optional<ClientOptions> parse_options(const std::vector<std::string>& args)
    {
        constexpr int default_max_reconnect_delay{ 30 };
        argparse::ArgumentParser parser{ "tunnelpp-client", TUNNELPP_VERSION };
        parser.add_argument("--server")
            .help("tunnel server WebSocket URL (ws:// or wss://), e.g. ws://tunnel.example.com/tunnel")
            .required();
        parser.add_argument("--token").help("authentication token shared with the server").required();
        parser.add_argument("--local")
            .help("local service URL to expose (http:// or https://), e.g. http://127.0.0.1:3000")
            .default_value(std::string{ "http://127.0.0.1:8080" });
        parser.add_argument("--subdomain")
            .help("requested subdomain (a random one is used when omitted)")
            .default_value(std::string{});
        parser.add_argument("--max-reconnect-delay")
            .help("maximum delay between reconnection attempts, in seconds")
            .default_value(default_max_reconnect_delay)
            .scan<'i', int>();
        parser.add_argument("--verbose").help("enable debug logging").default_value(false).implicit_value(true);

        try {
            parser.parse_args(args);
        } catch (const std::exception& error) {
            std::cerr << "error: " << error.what() << '\n';
            return std::nullopt;
        }

        ClientOptions options;
        options.server_url = parser.get<std::string>("--server");
        options.token = parser.get<std::string>("--token");
        options.max_reconnect_delay_seconds = parser.get<int>("--max-reconnect-delay");
        options.verbose = parser.get<bool>("--verbose");
        if (parser.is_used("--subdomain")) {
            options.subdomain = parser.get<std::string>("--subdomain");
        }

        const auto server{ parse_url(options.server_url) };
        if (!server || (server->scheme != "ws" && server->scheme != "wss")) {
            std::cerr << "error: --server must be a ws:// or wss:// URL\n";
            return std::nullopt;
        }
        const auto local{ parse_url(parser.get<std::string>("--local")) };
        if (!local || (local->scheme != "http" && local->scheme != "https")) {
            std::cerr << "error: --local must be an http:// or https:// URL\n";
            return std::nullopt;
        }
        options.local = *local;
        if (!options.subdomain.empty() && !is_valid_subdomain(options.subdomain)) {
            std::cerr << "error: --subdomain must be 3-63 lowercase letters, digits or hyphens\n";
            return std::nullopt;
        }
        return options;
    }
}
