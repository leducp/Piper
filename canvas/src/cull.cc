#include "piper/canvas/cull.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

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
