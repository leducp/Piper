#include "piper/canvas/editor.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "piper/canvas/graph.h"

namespace piper::canvas
{
    namespace
    {
        constexpr float zoom_min  = 0.1f;
        constexpr float zoom_max  = 10.0f;
        constexpr float zoom_step = 1.1f;
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

        // Pan: middle-mouse drag, no threshold so any drag counts.
        if (hovered and ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
        {
            ImVec2 const delta = ImGui::GetIO().MouseDelta;
            transform_.pan.x -= delta.x / transform_.zoom;
            transform_.pan.y -= delta.y / transform_.zoom;
        }

        // Zoom: mouse wheel around cursor (canvas point under cursor
        // stays under cursor across the zoom).
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

        dl->PopClipRect();

        // PR 2.3+: render nodes via source_.nodes(), links via
        // source_.links().
        (void)source_;
    }

    std::span<Event const> Editor::consume_events()
    {
        drained_events_.clear();
        pending_events_.swap(drained_events_);
        return drained_events_;
    }

    void Editor::center_on(NodeId id)
    {
        // PR 2.3 fills this in once nodes are queryable by id.
        (void)id;
    }

    void Editor::scroll_to(NodeId id)
    {
        // PR 2.3 fills this in.
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
