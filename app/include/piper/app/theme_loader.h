#ifndef PIPER_APP_THEME_LOADER_H
#define PIPER_APP_THEME_LOADER_H

#include <string>

#include <imgui.h>

#include "piper/canvas/style.h"
#include "piper/color.h"
#include "piper/theme.h"

namespace piper::app
{
    // Translates a domain-side piper::Theme into the canvas
    // framework's Style and applies a set of ImGuiStyle overrides
    // for a coherent app-wide look. The host is responsible for
    // calling this every time the theme changes (including on hot
    // reload).
    void apply_theme(piper::Theme const& theme,
                     canvas::Style&      canvas_style,
                     ImGuiStyle&         imgui_style);

    // Looks up a type's color in the theme; falls back to a stable
    // pastel hue derived from the type name's hash so unknown types
    // still get distinct, reproducible colors.
    rgba color_for_type(piper::Theme const& theme, std::string const& data_type);

    // Encodes a piper::rgba (RRGGBBAA) into ImGui's IM_COL32 byte
    // order. Direct casts swizzle, so always go through this helper.
    inline ImU32 to_imu32(rgba c)
    {
        return IM_COL32(c.r(), c.g(), c.b(), c.a());
    }
}

#endif
