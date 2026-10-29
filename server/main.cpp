#include "tunnel_server.h"

#include <config.h>
#include <logging.h>

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <thread>

static constexpr int poll_ms{ 100 };

static std::atomic<bool>& quit_flag()
{
    static std::atomic<bool> flag{ false };
    return flag;
}

static void on_signal([[maybe_unused]] int signum)
{
    quit_flag() = true;
}

int main()
{
    logging::init(false);
    const auto config{ tunnelpp::load_server_config() };
    if (!config) {
        spdlog::error("invalid configuration: TUNNELPP_TOKEN is required (see docs/server.md)");
        return 1;
    }

    tunnelpp::server::TunnelServer server{ *config };
    if (!server.start()) {
        return 1;
    }
    spdlog::info("tunnelpp-server {} listening", TUNNELPP_VERSION);
    spdlog::info("public listener on {}:{}", config->host, server.public_port());
    spdlog::info("admin listener on {}:{}", config->host, server.admin_port());
    spdlog::info("domain: {}", config->domain);

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    while (!quit_flag()) {
        std::this_thread::sleep_for(std::chrono::milliseconds{ poll_ms });
    }

    spdlog::info("shutting down");
    server.stop();
    return 0;
}
