#ifndef PIPER_THEME_H
#define PIPER_THEME_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "piper/color.h"
#include "piper/diagnostic.h"

namespace piper
{
    struct Theme
    {
        // Canvas
        rgba  canvas_bg{ rgba::from_components(0x1A, 0x1A, 0x1A, 0xFF) };
        rgba  grid_line{ rgba::from_components(0x2A, 0x2A, 0x2A, 0xFF) };
        float grid_spacing{30.0f};

        // Nodes
        rgba  node_default_header{ rgba::from_components(0x3A, 0x3A, 0x3A, 0xFF) };
        rgba  node_default_body{   rgba::from_components(0x2A, 0x2A, 0x2A, 0xFF) };
        float node_rounding{4.0f};
        float node_body_alpha_disabled{0.35f};
        float pin_alpha_inactive{0.25f};

        // Links
        float link_thickness{2.0f};
        float link_bezier_strength{50.0f};
        rgba  link_invalid{ rgba::from_components(0xFF, 0x40, 0x40, 0xFF) };

        // Per-data-type pin/link colors keyed by AttributeSpec::data_type.
        std::unordered_map<std::string, rgba> type_colors;

        // mode_color_table — keyed by ModeProfile::per_node label.
        // Built-ins ("enable", "disable") are pre-populated by load_theme;
        // host applications may add custom labels at runtime.
        std::unordered_map<std::string, rgba> mode_colors;
    };

    struct ThemeLoadResult
    {
        Theme                   theme;
        std::vector<Diagnostic> diagnostics;
    };

    // Throws std::runtime_error on read errors, malformed JSON, or
    // unsupported version. Schema-level issues are diagnostics.
    ThemeLoadResult load_theme(std::string_view path);

    // Throws std::runtime_error on malformed JSON or unsupported
    // version. Schema-level issues are diagnostics.
    ThemeLoadResult load_theme_from_string(std::string_view json);
}

#endif
