#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "piper/canvas/cull.h"

namespace piper::canvas
{
    namespace
    {
        // Width of a pin label at the unscaled (zoom 1) font, or 0
        // when the label is empty or no ImGui context is active.
        // Bare ImGui::CalcTextSize works fine at any frame stage as
        // long as a context exists; tests run without one and skip.
        float label_width(std::string_view label)
        {
            if (label.empty() or ImGui::GetCurrentContext() == nullptr)
            {
                return 0.0f;
            }
            return ImGui::CalcTextSize(label.data(),
                                       label.data() + label.size()).x;
        }
    }

    float node_total_width(Node const& node, LayoutMetrics const& metrics)
    {
        float w = std::max(node.body_min_size.x, metrics.min_width);

        // Auto-fit pin labels: each row may carry both an input
        // label (left-anchored) and an output label (right-
        // anchored), so the body must be wide enough that they do
        // not overlap.
        std::size_t const rows = std::max(node.inputs.size(), node.outputs.size());
        for (std::size_t r = 0; r < rows; ++r)
        {
            float in_w  = 0.0f;
            float out_w = 0.0f;
            if (r < node.inputs.size())
            {
                in_w = label_width(node.inputs[r].label);
            }
            if (r < node.outputs.size())
            {
                out_w = label_width(node.outputs[r].label);
            }
            float const needed = in_w + out_w + metrics.label_padding;
            if (needed > w)
            {
                w = needed;
            }
        }
        return w;
    }

    Aabb node_aabb(Node const& node, LayoutMetrics const& metrics)
    {
        if (node.shape != Shape::Rect)
        {
            // Compact label pentagon: title text + padding, plus the
            // chevron-tip extent on the abstraction side. Padding
            // matches style.node_padding so the title sits flush.
            constexpr float pad_x = 8.0f;
            constexpr float pad_y = 6.0f;
            ImVec2 title_size{ 0.0f, 0.0f };
            if (not node.title.empty() and ImGui::GetCurrentContext() != nullptr)
            {
                title_size = ImGui::CalcTextSize(node.title.data(),
                                                  node.title.data() + node.title.size());
            }
            float const min_dim = metrics.pin_row_height;
            float const body_h  = std::max(title_size.y + 2.0f * pad_y, min_dim);
            float const tip     = body_h * 0.5f;
            float const body_w  = std::max(title_size.x + 2.0f * pad_x, min_dim) + tip;
            return Aabb{
                node.pos,
                ImVec2{ node.pos.x + body_w, node.pos.y + body_h },
            };
        }

        // Body height = pin rows + host-declared extra content,
        // floored by min_body_height so a node with no pins and no
        // extra content still has a clickable body.
        std::size_t const pin_rows      = std::max(node.inputs.size(), node.outputs.size());
        float const       pin_content_h = float(pin_rows) * metrics.pin_row_height;
        float const       extra         = std::max(0.0f, node.body_min_size.y);
        float const       content_h     = std::max(pin_content_h + extra, metrics.min_body_height);

        float const total_h = metrics.header_height + content_h;
        float const total_w = node_total_width(node, metrics);

        return Aabb{
            node.pos,
            ImVec2{ node.pos.x + total_w, node.pos.y + total_h },
        };
    }

    std::vector<std::size_t> cull_visible(
        std::span<Node const>  nodes,
        Aabb const&            viewport,
        LayoutMetrics const&   metrics)
    {
        std::vector<std::size_t> result;
        result.reserve(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            if (node_aabb(nodes[i], metrics).intersects(viewport))
            {
                result.push_back(i);
            }
        }
        return result;
    }

    ImVec2 pin_center_in_node(Node const& node,
                              PinKind kind,
                              std::size_t index,
                              LayoutMetrics const& metrics)
    {
        if (node.shape != Shape::Rect)
        {
            // Label pentagons hang their single pin on the flat edge
            // (opposite the chevron tip), centered on body mid-height.
            Aabb const a     = node_aabb(node, metrics);
            float const mid_y = (a.min.y + a.max.y) * 0.5f;
            if (node.shape == Shape::LabelIn)
            {
                return ImVec2{ a.min.x, mid_y };
            }
            return ImVec2{ a.max.x, mid_y };
        }
        float const y = node.pos.y
                      + metrics.header_height
                      + (float(index) + 0.5f) * metrics.pin_row_height;
        if (kind == PinKind::Input)
        {
            return ImVec2{ node.pos.x, y };
        }
        return ImVec2{ node.pos.x + node_total_width(node, metrics), y };
    }

    BezierPoints link_bezier(ImVec2 const& a, ImVec2 const& b, float strength)
    {
        float const ext = std::max(std::abs(b.x - a.x) * 0.5f, strength);
        return BezierPoints{
            a,
            ImVec2{ a.x + ext, a.y },
            ImVec2{ b.x - ext, b.y },
            b,
        };
    }
}
