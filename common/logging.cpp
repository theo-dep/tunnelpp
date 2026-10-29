#include "logging.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace logging
{
    void init(bool debug)
    {
        auto logger{ spdlog::stdout_color_mt("tunnelpp") };
        logger->set_level(debug ? spdlog::level::debug : spdlog::level::info);
        logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%s:%#] %v");
        spdlog::set_default_logger(std::move(logger));
    }
}
