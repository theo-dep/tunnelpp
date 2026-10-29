#pragma once

#include <httplib.h>

namespace tunnelpp
{
    httplib::Headers filter_tunnel_headers(const httplib::Headers& headers);
}
