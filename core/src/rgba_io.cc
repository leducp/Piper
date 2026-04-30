#include "piper/rgba_io.h"

#include <stdint.h>
#include <cstdio>

namespace piper
{
    std::optional<rgba> parse_rgba(std::string_view s)
    {
        if (s.size() != 9 or s[0] != '#')
        {
            return std::nullopt;
        }
        uint32_t v = 0;
        for (std::size_t i = 1; i < s.size(); ++i)
        {
            char ch = s[i];
            uint32_t d = 0;
            if (ch >= '0' and ch <= '9')
            {
                d = uint32_t(ch - '0');
            }
            else if (ch >= 'a' and ch <= 'f')
            {
                d = uint32_t(ch - 'a' + 10);
            }
            else if (ch >= 'A' and ch <= 'F')
            {
                d = uint32_t(ch - 'A' + 10);
            }
            else
            {
                return std::nullopt;
            }
            v = (v << 4) | d;
        }
        return rgba{ v };
    }

    std::string format_rgba(rgba c)
    {
        char buf[12];
        std::snprintf(buf, sizeof(buf), "#%08X", c.value);
        return std::string(buf);
    }
}
