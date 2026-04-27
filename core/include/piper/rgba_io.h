#ifndef PIPER_RGBA_IO_H
#define PIPER_RGBA_IO_H

#include <optional>
#include <string>
#include <string_view>

#include "piper/color.h"

namespace piper
{
    // Accepts "#RRGGBBAA" (alpha mandatory, case-insensitive).
    std::optional<rgba> parse_rgba(std::string_view s);

    // Emits "#RRGGBBAA" upper-case.
    std::string format_rgba(rgba c);
}

#endif
