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
    // Layout metrics used for AABB and pin-position computation.
    // Mirrored as render constants in editor.cc so cull, render, and
    // hit-test agree.
    struct LayoutMetrics
    {
        float header_height{24.0f};
        float pin_row_height{18.0f};
        float min_width{120.0f};
        float min_body_height{30.0f};
        // Horizontal slack reserved on each row for pin radii, the
        // gap between pin and label, and a minimum visual gutter
        // between an input label and an output label.
        float label_padding{24.0f};
    };

    Aabb node_aabb(Node const& node, LayoutMetrics const& metrics);

    // Returns indices into `nodes` for entries whose AABB intersects
    // `viewport` (canvas-space).
    std::vector<std::size_t> cull_visible(
        std::span<Node const>  nodes,
        Aabb const&            viewport,
        LayoutMetrics const&   metrics);

    // Overload over precomputed AABBs; returns indices into `aabbs`.
    std::vector<std::size_t> cull_visible(
        std::span<Aabb const> aabbs,
        Aabb const&           viewport);

    // Canvas-space center of the i-th pin of the given kind on `node`.
    // Inputs are pinned to the node's left edge, outputs to the right.
    // Vertically: header_height + (i + 0.5) * pin_row_height.
    ImVec2 pin_center_in_node(Node const& node,
                              PinKind kind,
                              std::size_t index,
                              LayoutMetrics const& metrics);

    // Overload taking the node's precomputed node_aabb(); skips the
    // text re-measure.
    ImVec2 pin_center_in_node(Node const& node,
                              PinKind kind,
                              std::size_t index,
                              LayoutMetrics const& metrics,
                              Aabb const& aabb);

    struct BezierPoints
    {
        ImVec2 a;
        ImVec2 c1;
        ImVec2 c2;
        ImVec2 b;
    };

    // Cubic-bezier control points for a horizontal-flow link from `a`
    // (output) to `b` (input). Strength is the minimum horizontal
    // extension of the control points; longer links get a proportional
    // sweep.
    BezierPoints link_bezier(ImVec2 const& a, ImVec2 const& b, float strength);
}

#endif
