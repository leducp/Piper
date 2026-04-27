#ifndef PIPER_CANVAS_CULL_H
#define PIPER_CANVAS_CULL_H

#include <cstddef>
#include <span>
#include <vector>

#include "piper/canvas/aabb.h"
#include "piper/canvas/graph.h"
#include "piper/canvas/style.h"

namespace piper::canvas
{
    // Layout metrics used for AABB computation. Mirrored as render
    // constants in editor.cc so cull and render agree.
    struct LayoutMetrics
    {
        float header_height{24.0f};
        float pin_row_height{18.0f};
        float min_width{120.0f};
        float min_body_height{30.0f};
    };

    Aabb node_aabb(Node const& node, LayoutMetrics const& metrics);

    // Returns indices into `nodes` for entries whose AABB intersects
    // `viewport` (canvas-space).
    std::vector<std::size_t> cull_visible(
        std::span<Node const>  nodes,
        Aabb const&            viewport,
        LayoutMetrics const&   metrics);
}

#endif
