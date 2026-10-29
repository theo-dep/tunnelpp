#pragma once

#include <httplib.h>

#include <optional>
#include <string>
#include <string_view>

namespace tunnelpp
{
    constexpr int protocol_version{ 1 };

    enum class MessageType
    {
        Register,
        Registered,
        Request,
        Response,
        Error,
        Ping,
        Pong,
    };

    struct Message
    {
        MessageType type{ MessageType::Ping };
        std::string id;           // request/response/error correlation
        int version{ 0 };         // register
        std::string token;        // register
        std::string subdomain;    // register/registered
        std::string url;          // registered
        std::string method;       // request
        std::string path;         // request, includes the query string
        httplib::Headers headers; // request/response
        std::string body;         // request/response, raw bytes
        int status{ 0 };          // response
        std::string code;         // error
        std::string detail;       // error
    };

    std::string encode_message(const Message& message);
    std::optional<Message> decode_message(std::string_view data);
}
