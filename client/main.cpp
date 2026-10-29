#include "args.h"
#include "tunnel_client.h"

#include <logging.h>

#include <spdlog/spdlog.h>

#include <csignal>
#include <span>
#include <string>
#include <vector>

static tunnelpp::client::TunnelClient*& client_instance()
{
    static tunnelpp::client::TunnelClient* instance{ nullptr };
    return instance;
}

static void on_signal([[maybe_unused]] int signum)
{
    if (auto* client = client_instance()) {
        client->stop();
    }
}

int main(int argc, char** argv)
{
    const std::span raw_args{ argv, static_cast<std::size_t>(argc) };
    std::vector<std::string> args;
    for (const char* arg : raw_args) {
        args.emplace_back(arg);
    }
    const auto options{ tunnelpp::client::parse_options(args) };
    if (!options) {
        return 1;
    }
    logging::init(options->verbose);
    spdlog::info("tunnelpp-client {} starting", TUNNELPP_VERSION);

    tunnelpp::client::ClientConfig config;
    config.server_url = options->server_url;
    config.token = options->token;
    config.local = options->local;
    config.subdomain = options->subdomain;
    config.max_reconnect_delay_seconds = options->max_reconnect_delay_seconds;

    tunnelpp::client::TunnelClient client{ config };
    client_instance() = &client;
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    client.run();
    client_instance() = nullptr;
    return client.fatal() ? 1 : 0;
}
