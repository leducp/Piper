#ifndef PIPER_CANVAS_STYLE_H
#define PIPER_CANVAS_STYLE_H

#include <imgui.h>

namespace piper::canvas
{
    // ImU32 here is ImGui's IM_COL32 encoding (A<<24 | B<<16 | G<<8 | R
    // on little-endian). The host's theme loader converts from any
    // other byte order (e.g. piper::rgba's RRGGBBAA) via
    // IM_COL32(c.r(), c.g(), c.b(), c.a()) — direct casts swizzle.
    struct Style
    {
        ImU32  canvas_bg{IM_COL32(0x1A, 0x1A, 0x1A, 0xFF)};
        ImU32  grid_line{IM_COL32(0x2A, 0x2A, 0x2A, 0xFF)};
        float  grid_spacing{30.0f};

        ImU32  node_default_header{IM_COL32(0x3A, 0x3A, 0x3A, 0xFF)};
        ImU32  node_default_body{IM_COL32(0x2A, 0x2A, 0x2A, 0xFF)};
        ImU32  node_outline{IM_COL32(0x44, 0x44, 0x44, 0xFF)};
        ImU32  node_outline_selected{IM_COL32(0xFF, 0xC0, 0x40, 0xFF)};
        float  node_rounding{4.0f};
        ImVec2 node_padding{8.0f, 6.0f};

        float  pin_radius{4.0f};

        float  link_thickness{2.0f};
        float  link_bezier_strength{50.0f};
        ImU32  link_invalid{IM_COL32(0xFF, 0x40, 0x40, 0xFF)};
        ImU32  link_default{IM_COL32(0xC0, 0xC0, 0xC0, 0xFF)};

        ImU32  selection_box{IM_COL32(0xFF, 0xC0, 0x40, 0x40)};
    };
}

#endif
