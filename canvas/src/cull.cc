#include "piper/canvas/cull.h"

#include <algorithm>

namespace piper::canvas
{
    Aabb node_aabb(Node const& node, LayoutMetrics const& metrics)
    {
        std::size_t const pin_rows = std::max(node.inputs.size(), node.outputs.size());
        float const pin_content_h  = float(pin_rows) * metrics.pin_row_height;

        float const content_h = std::max(
            { node.body_min_size.y, pin_content_h, metrics.min_body_height });
        float const total_h   = metrics.header_height + content_h;
        float const total_w   = std::max(node.body_min_size.x, metrics.min_width);

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
}
