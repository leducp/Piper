#include <functional>

#include "piper/app/theme_loader.h"

namespace piper::studio
{
    void apply_theme(piper::Theme const& theme,
                     canvas::Style&      canvas_style,
                     ImGuiStyle&         imgui_style)
    {
        canvas_style.canvas_bg            = to_imu32(theme.canvas_bg);
        canvas_style.grid_line            = to_imu32(theme.grid_line);
        canvas_style.grid_spacing         = theme.grid_spacing;
        canvas_style.node_default_header  = to_imu32(theme.node_default_header);
        canvas_style.node_default_body    = to_imu32(theme.node_default_body);
        canvas_style.node_rounding        = theme.node_rounding;
        canvas_style.link_thickness       = theme.link_thickness;
        canvas_style.link_bezier_strength = theme.link_bezier_strength;
        canvas_style.link_invalid         = to_imu32(theme.link_invalid);

        // ImGuiStyle: keep the defaults from StyleColorsDark; only
        // override the surfaces that should match the canvas. Window
        // and frame backgrounds pick up the canvas/body palette so
        // the menu bar reads as part of the same scene.
        imgui_style.WindowRounding        = 0.0f;
        imgui_style.FrameRounding         = theme.node_rounding;
        imgui_style.GrabRounding          = theme.node_rounding;

        ImVec4 const window_bg = ImGui::ColorConvertU32ToFloat4(to_imu32(theme.canvas_bg));
        ImVec4 const frame_bg  = ImGui::ColorConvertU32ToFloat4(to_imu32(theme.node_default_body));
        imgui_style.Colors[ImGuiCol_WindowBg] = window_bg;
        imgui_style.Colors[ImGuiCol_MenuBarBg] = window_bg;
        imgui_style.Colors[ImGuiCol_PopupBg]   = frame_bg;
    }

    rgba color_for_type(piper::Theme const& theme, std::string const& data_type)
    {
        auto const it = theme.type_colors.find(data_type);
        if (it != theme.type_colors.end())
        {
            return it->second;
        }
        // Fallback: deterministic pastel from hashed type name.
        std::size_t const h = std::hash<std::string>{}(data_type);
        return pastel_from_hue_index(int(h % 32u));
    }
}
