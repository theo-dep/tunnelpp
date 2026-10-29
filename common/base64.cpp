#include "base64.h"

#include <array>

namespace tunnelpp
{
    static constexpr std::string_view chars{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" };

    static int decode_char(char c)
    {
        const auto pos{ chars.find(c) };
        return pos == std::string_view::npos ? -1 : static_cast<int>(pos);
    }

    std::string base64_encode(std::string_view data)
    {
        constexpr std::size_t group{ 3 };
        constexpr int low_pair{ 0x03 };
        constexpr int low_nibble{ 0x0f };
        constexpr int sextet_mask{ 0x3f };
        constexpr int sextet_shift{ 6 };
        std::string output;
        output.reserve((data.size() + group - 1) / group * 4);
        for (std::size_t i = 0; i < data.size(); i += group) {
            const auto a{ static_cast<unsigned char>(data.at(i)) };
            const auto b{ i + 1 < data.size() ? static_cast<unsigned char>(data.at(i + 1)) : 0 };
            const auto c{ i + 2 < data.size() ? static_cast<unsigned char>(data.at(i + 2)) : 0 };
            output.push_back(chars.at(a >> 2));
            output.push_back(chars.at(((a & low_pair) << 4) | (b >> 4)));
            output.push_back(i + 1 < data.size() ? chars.at(((b & low_nibble) << 2) | (c >> sextet_shift)) : '=');
            output.push_back(i + 2 < data.size() ? chars.at(c & sextet_mask) : '=');
        }
        return output;
    }

    std::optional<std::string> base64_decode(std::string_view data)
    {
        constexpr std::size_t group{ 4 };
        constexpr int low_nibble{ 0x0f };
        constexpr int low_pair{ 0x03 };
        constexpr int sextet_shift{ 6 };
        if (data.size() % group != 0) {
            return std::nullopt;
        }
        std::string output;
        output.reserve(data.size() / group * 3);
        for (std::size_t i = 0; i < data.size(); i += group) {
            std::array<int, group> values{};
            int padding{ 0 };
            for (std::size_t j = 0; j < values.size(); ++j) {
                const char c{ data.at(i + j) };
                if (c == '=') {
                    if (i + group != data.size() || j < 2) {
                        return std::nullopt;
                    }
                    ++padding;
                } else {
                    if (padding > 0) {
                        return std::nullopt;
                    }
                    values.at(j) = decode_char(c);
                    if (values.at(j) < 0) {
                        return std::nullopt;
                    }
                }
            }
            output.push_back(static_cast<char>((values.at(0) << 2) | (values.at(1) >> 4)));
            if (padding < 2) {
                output.push_back(static_cast<char>(((values.at(1) & low_nibble) << 4) | (values.at(2) >> 2)));
            }
            if (padding < 1) {
                output.push_back(static_cast<char>(((values.at(2) & low_pair) << sextet_shift) | values.at(3)));
            }
        }
        return output;
    }
}
