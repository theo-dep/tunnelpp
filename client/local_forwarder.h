#pragma once

#include <protocol.h>
#include <url.h>

namespace tunnelpp::client
{
    Message forward_to_local(const Url& local, const Message& request, int timeout_seconds);
}
