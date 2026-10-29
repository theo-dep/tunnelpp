#include "protocol.h"

#include "base64.h"

#include <nlohmann/json.hpp>

namespace tunnelpp
{
    static std::optional<MessageType> type_from_name(std::string_view name)
    {
        if (name == "register") {
            return MessageType::Register;
        }
        if (name == "registered") {
            return MessageType::Registered;
        }
        if (name == "request") {
            return MessageType::Request;
        }
        if (name == "response") {
            return MessageType::Response;
        }
        if (name == "error") {
            return MessageType::Error;
        }
        if (name == "ping") {
            return MessageType::Ping;
        }
        if (name == "pong") {
            return MessageType::Pong;
        }
        return std::nullopt;
    }

    static nlohmann::json headers_to_json(const httplib::Headers& headers)
    {
        auto result = nlohmann::json::array();
        for (const auto& [key, value] : headers) {
            result.push_back({ key, value });
        }
        return result;
    }

    static httplib::Headers headers_from_json(const nlohmann::json& json)
    {
        httplib::Headers headers;
        for (const auto& entry : json) {
            headers.emplace(entry.at(0).get<std::string>(), entry.at(1).get<std::string>());
        }
        return headers;
    }

    static bool decode_body(const nlohmann::json& json, Message& message)
    {
        if (!json.contains("body_b64")) {
            return true;
        }
        auto body{ base64_decode(json.at("body_b64").get<std::string>()) };
        if (!body) {
            return false;
        }
        message.body = std::move(*body);
        return true;
    }

    static bool decode_headers(const nlohmann::json& json, Message& message)
    {
        if (!json.contains("headers")) {
            return true;
        }
        if (!json.at("headers").is_array()) {
            return false;
        }
        message.headers = headers_from_json(json.at("headers"));
        return true;
    }

    std::string encode_message(const Message& message)
    {
        switch (message.type) {
            case MessageType::Register:
                return nlohmann::json{
                    { "type", "register" },
                    { "version", message.version },
                    { "subdomain", message.subdomain },
                    { "token", message.token },
                }
                    .dump();
            case MessageType::Registered:
                return nlohmann::json{
                    { "type", "registered" },
                    { "subdomain", message.subdomain },
                    { "url", message.url },
                }
                    .dump();
            case MessageType::Request:
                return nlohmann::json{
                    { "type", "request" },
                    { "id", message.id },
                    { "method", message.method },
                    { "path", message.path },
                    { "headers", headers_to_json(message.headers) },
                    { "body_b64", base64_encode(message.body) },
                }
                    .dump();
            case MessageType::Response:
                return nlohmann::json{
                    { "type", "response" },
                    { "id", message.id },
                    { "status", message.status },
                    { "headers", headers_to_json(message.headers) },
                    { "body_b64", base64_encode(message.body) },
                }
                    .dump();
            case MessageType::Error:
                return nlohmann::json{
                    { "type", "error" },
                    { "id", message.id },
                    { "code", message.code },
                    { "message", message.detail },
                }
                    .dump();
            case MessageType::Ping:
                return R"({"type":"ping"})";
            case MessageType::Pong:
                return R"({"type":"pong"})";
        }
        return "";
    }

    std::optional<Message> decode_message(std::string_view data)
    {
        const auto json = nlohmann::json::parse(data, nullptr, false);
        if (json.is_discarded() || !json.is_object() || !json.contains("type") || !json.at("type").is_string()) {
            return std::nullopt;
        }
        const auto type{ type_from_name(json.at("type").get<std::string>()) };
        if (!type) {
            return std::nullopt;
        }
        try {
            Message message;
            message.type = *type;
            switch (*type) {
                case MessageType::Register:
                    message.version = json.at("version").get<int>();
                    message.subdomain = json.at("subdomain").get<std::string>();
                    message.token = json.at("token").get<std::string>();
                    break;
                case MessageType::Registered:
                    message.subdomain = json.at("subdomain").get<std::string>();
                    message.url = json.at("url").get<std::string>();
                    break;
                case MessageType::Request:
                    message.id = json.at("id").get<std::string>();
                    message.method = json.at("method").get<std::string>();
                    message.path = json.at("path").get<std::string>();
                    if (!decode_headers(json, message) || !decode_body(json, message)) {
                        return std::nullopt;
                    }
                    break;
                case MessageType::Response:
                    message.id = json.at("id").get<std::string>();
                    message.status = json.at("status").get<int>();
                    if (!decode_headers(json, message) || !decode_body(json, message)) {
                        return std::nullopt;
                    }
                    break;
                case MessageType::Error:
                    message.id = json.value("id", std::string{});
                    message.code = json.at("code").get<std::string>();
                    message.detail = json.at("message").get<std::string>();
                    break;
                case MessageType::Ping:
                case MessageType::Pong:
                    break;
            }
            return message;
        } catch (const nlohmann::json::exception&) {
            return std::nullopt;
        }
    }
}
