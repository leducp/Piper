#include "piper/canvas/editor.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "piper/canvas/aabb.h"
#include "piper/canvas/cull.h"
#include "piper/canvas/graph.h"

namespace piper::canvas
{
    namespace
    {
        constexpr float zoom_min  = 0.1f;
        constexpr float zoom_max  = 10.0f;
        constexpr float zoom_step = 1.1f;

        constexpr LayoutMetrics layout{};

        ImU32 apply_alpha(ImU32 color, float alpha)
        {
            float const clamped = std::clamp(alpha, 0.0f, 1.0f);
            uint32_t const original_a = (color >> IM_COL32_A_SHIFT) & 0xFFu;
            uint32_t const scaled_a   = uint32_t(float(original_a) * clamped);
            return (color & ~uint32_t(IM_COL32_A_MASK))
                 | (scaled_a << IM_COL32_A_SHIFT);
        }

        void draw_node(ImDrawList* dl,
                       Node const& node,
                       Style const& style,
                       Transform const& transform,
                       ImVec2 origin)
        {
            Aabb const local      = node_aabb(node, layout);
            ImVec2 const top_left = transform.to_screen(local.min, origin);
            ImVec2 const bot_right = transform.to_screen(local.max, origin);

            float const header_h_screen = layout.header_height * transform.zoom;
            ImVec2 const header_br{ bot_right.x, top_left.y + header_h_screen };

            ImU32 const body_color = apply_alpha(node.body_color, node.body_alpha);

            // Body fill (rounded all corners)
            dl->AddRectFilled(top_left, bot_right, body_color, style.node_rounding);

            // Header (rounded top corners only — overlays the body)
            dl->AddRectFilled(top_left, header_br, node.header_color,
                              style.node_rounding, ImDrawFlags_RoundCornersTop);

            // Outline
            dl->AddRect(top_left, bot_right, style.node_outline, style.node_rounding);

            if (not node.title.empty())
            {
                ImVec2 const title_pos{
                    top_left.x + style.node_padding.x,
                    top_left.y + style.node_padding.y,
                };
                dl->AddText(title_pos, IM_COL32_WHITE,
                            node.title.data(),
                            node.title.data() + node.title.size());
            }
        }
    }

    Editor::Editor(Graph& source)
        : source_(source)
    {
    }

    void Editor::draw(ImVec2 size)
    {
        ImVec2 const origin{ImGui::GetCursorScreenPos()};
        last_origin_ = origin;
        ImVec2 const br{ origin.x + size.x, origin.y + size.y };

        ImGui::InvisibleButton("##canvas", size,
            ImGuiButtonFlags_MouseButtonLeft   |
            ImGuiButtonFlags_MouseButtonRight  |
            ImGuiButtonFlags_MouseButtonMiddle);

        bool const hovered = ImGui::IsItemHovered();

        if (hovered and ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
        {
            ImVec2 const delta = ImGui::GetIO().MouseDelta;
            transform_.pan.x -= delta.x / transform_.zoom;
            transform_.pan.y -= delta.y / transform_.zoom;
        }

        float const wheel = ImGui::GetIO().MouseWheel;
        if (hovered and wheel != 0.0f)
        {
            ImVec2 const mouse = ImGui::GetIO().MousePos;
            ImVec2 const canvas_before = transform_.to_canvas(mouse, origin);

            float const factor = std::pow(zoom_step, wheel);
            transform_.zoom = std::clamp(transform_.zoom * factor, zoom_min, zoom_max);

            ImVec2 const canvas_after = transform_.to_canvas(mouse, origin);
            transform_.pan.x += canvas_before.x - canvas_after.x;
            transform_.pan.y += canvas_before.y - canvas_after.y;
        }

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(origin, br, style_.canvas_bg);
        dl->PushClipRect(origin, br, true);

        // Grid
        ImVec2 const canvas_min = transform_.to_canvas(origin, origin);
        ImVec2 const canvas_max = transform_.to_canvas(br, origin);
        float  const spacing    = style_.grid_spacing;

        for (float x = std::floor(canvas_min.x / spacing) * spacing;
             x < canvas_max.x;
             x += spacing)
        {
            ImVec2 const a = transform_.to_screen({ x, canvas_min.y }, origin);
            ImVec2 const b = transform_.to_screen({ x, canvas_max.y }, origin);
            dl->AddLine(a, b, style_.grid_line);
        }

        for (float y = std::floor(canvas_min.y / spacing) * spacing;
             y < canvas_max.y;
             y += spacing)
        {
            ImVec2 const a = transform_.to_screen({ canvas_min.x, y }, origin);
            ImVec2 const b = transform_.to_screen({ canvas_max.x, y }, origin);
            dl->AddLine(a, b, style_.grid_line);
        }

        // Nodes (cull then render)
        Aabb const viewport{ canvas_min, canvas_max };
        auto const nodes = source_.nodes();
        auto const visible = cull_visible(nodes, viewport, layout);
        for (auto idx : visible)
        {
            draw_node(dl, nodes[idx], style_, transform_, origin);
        }

        dl->PopClipRect();
    }

    std::span<Event const> Editor::consume_events()
    {
        drained_events_.clear();
        pending_events_.swap(drained_events_);
        return drained_events_;
    }

    void Editor::center_on(NodeId id)
    {
        // PR 2.5 (selection-aware view) — needs node lookup by id.
        (void)id;
    }

    void Editor::scroll_to(NodeId id)
    {
        (void)id;
    }

    void Editor::set_selection(std::span<NodeId const> ids)
    {
        // PR 2.5 (selection).
        (void)ids;
    }

    ImVec2 Editor::screen_to_canvas(ImVec2 screen) const
    {
        return transform_.to_canvas(screen, last_origin_);
    }

    ImVec2 Editor::canvas_to_screen(ImVec2 canvas) const
    {
        return transform_.to_screen(canvas, last_origin_);
    }
}
