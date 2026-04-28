#include "piper/canvas/cull.h"

#include <algorithm>
#include <cmath>

namespace piper::canvas
{
    float node_total_width(Node const& node, LayoutMetrics const& metrics)
    {
        return std::max(node.body_min_size.x, metrics.min_width);
    }

    Aabb node_aabb(Node const& node, LayoutMetrics const& metrics)
    {
        std::size_t const pin_rows = std::max(node.inputs.size(), node.outputs.size());
        float const pin_content_h  = float(pin_rows) * metrics.pin_row_height;

        float const content_h = std::max({ node.body_min_size.y, pin_content_h, metrics.min_body_height });
        float const total_h   = metrics.header_height + content_h;
        float const total_w   = node_total_width(node, metrics);

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
