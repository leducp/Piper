#include <algorithm>
#include <unordered_map>

#include <imgui.h>

#include "piper/app/minimap.h"

namespace piper::studio
{
    void draw_minimap(Document& doc,
                      canvas::LayoutMetrics const& layout,
                      float dpi_scale)
    {
        auto const nodes = doc.adapter.nodes();
        if (nodes.empty())
        {
            return;
        }

        canvas::Aabb overall = canvas::node_aabb(nodes[0], layout);
        for (std::size_t i = 1; i < nodes.size(); ++i)
        {
            canvas::Aabb const a = canvas::node_aabb(nodes[i], layout);
            overall.min.x = std::min(overall.min.x, a.min.x);
            overall.min.y = std::min(overall.min.y, a.min.y);
            overall.max.x = std::max(overall.max.x, a.max.x);
            overall.max.y = std::max(overall.max.y, a.max.y);
        }

        float const mm_w   = 200.0f * dpi_scale;
        float const mm_h   = 150.0f * dpi_scale;
        float const margin = 12.0f * dpi_scale;
        auto   const vp    = doc.editor.viewport();
        if (vp.size_screen.x < mm_w + 2.0f * margin
            or vp.size_screen.y < mm_h + 2.0f * margin)
        {
            return;
        }
        ImVec2 const mm_min{
            vp.origin_screen.x + vp.size_screen.x - mm_w - margin,
            vp.origin_screen.y + vp.size_screen.y - mm_h - margin,
        };
        ImVec2 const mm_max{ mm_min.x + mm_w, mm_min.y + mm_h };

        float const aabb_w = std::max(overall.max.x - overall.min.x, 1.0f);
        float const aabb_h = std::max(overall.max.y - overall.min.y, 1.0f);
        float const inset  = 4.0f;
        float const sx     = (mm_w - 2.0f * inset) / aabb_w;
        float const sy     = (mm_h - 2.0f * inset) / aabb_h;
        float const s      = std::min(sx, sy);
        float const ox     = mm_min.x + (mm_w - aabb_w * s) * 0.5f;
        float const oy     = mm_min.y + (mm_h - aabb_h * s) * 0.5f;

        auto canvas_to_mm = [&](ImVec2 const& cp)
        {
            return ImVec2{
                ox + (cp.x - overall.min.x) * s,
                oy + (cp.y - overall.min.y) * s,
            };
        };

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(mm_min, mm_max, IM_COL32(0x10, 0x10, 0x10, 0xC8), 4.0f);
        dl->AddRect(mm_min, mm_max, IM_COL32(0x55, 0x55, 0x55, 0xFF), 4.0f);

        struct EdgePoints { ImVec2 right; ImVec2 left; };
        std::unordered_map<piper::NodeId, EdgePoints> anchors;
        anchors.reserve(nodes.size());
        for (auto const& n : nodes)
        {
            canvas::Aabb const a  = canvas::node_aabb(n, layout);
            float        const cy = (a.min.y + a.max.y) * 0.5f;
            anchors[piper::NodeId(n.id.v)] = EdgePoints{
                ImVec2{ a.max.x, cy },
                ImVec2{ a.min.x, cy },
            };
            ImVec2 const tl = canvas_to_mm(a.min);
            ImVec2 const br = canvas_to_mm(a.max);
            dl->AddRectFilled(tl, br, n.header_color);
        }

        ImU32 const link_col = IM_COL32(0x80, 0x80, 0x80, 0xC0);
        for (auto const& l : doc.graph.links())
        {
            auto from_it = anchors.find(l.from.node);
            auto to_it   = anchors.find(l.to.node);
            if (from_it == anchors.end() or to_it == anchors.end())
            {
                continue;
            }
            ImVec2 const from = canvas_to_mm(from_it->second.right);
            ImVec2 const to   = canvas_to_mm(to_it->second.left);
            dl->AddLine(from, to, link_col, 1.0f);
        }

        if (vp.zoom > 0.0f)
        {
            ImVec2 const vp_min_canvas = vp.pan;
            ImVec2 const vp_max_canvas{
                vp.pan.x + vp.size_screen.x / vp.zoom,
                vp.pan.y + vp.size_screen.y / vp.zoom,
            };
            ImVec2 vp_min = canvas_to_mm(vp_min_canvas);
            ImVec2 vp_max = canvas_to_mm(vp_max_canvas);
            vp_min.x = std::clamp(vp_min.x, mm_min.x, mm_max.x);
            vp_min.y = std::clamp(vp_min.y, mm_min.y, mm_max.y);
            vp_max.x = std::clamp(vp_max.x, mm_min.x, mm_max.x);
            vp_max.y = std::clamp(vp_max.y, mm_min.y, mm_max.y);
            dl->AddRect(vp_min, vp_max, IM_COL32(0xFF, 0xC0, 0x40, 0xFF),
                        0.0f, 0, 1.5f);
        }

        if (ImGui::IsMouseHoveringRect(mm_min, mm_max)
            and ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            ImVec2 const m = ImGui::GetMousePos();
            doc.editor.center_on(ImVec2{
                overall.min.x + (m.x - ox) / s,
                overall.min.y + (m.y - oy) / s,
            });
        }
    }
}
