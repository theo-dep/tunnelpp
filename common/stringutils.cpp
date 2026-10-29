#include "stringutils.h"

namespace tunnelpp
{
    std::string lower(std::string_view text)
    {
        std::string result{ text };
        for (char& c : result) {
            if (c >= 'A' && c <= 'Z') {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        return result;
    }
}
