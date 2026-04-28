#ifndef PIPER_CANVAS_HIT_TEST_H
#define PIPER_CANVAS_HIT_TEST_H

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include <imgui.h>

#include "piper/canvas/aabb.h"
#include "piper/canvas/cull.h"
#include "piper/canvas/graph.h"
#include "piper/canvas/ids.h"

namespace piper::canvas
{
    // All hit_test inputs are canvas-space.

    // Returns the topmost node whose AABB contains `point`. Topmost =
    // last in `nodes` (later entries are drawn on top), so iteration
    // is reversed.
    std::optional<NodeId> hit_test_node(std::span<Node const>  nodes,
                                        ImVec2 const&          point,
                                        LayoutMetrics const&   metrics);

    // Indices into `nodes` whose AABB intersects `box` — used by
    // box-select.
    std::vector<std::size_t> nodes_in_box(std::span<Node const>  nodes,
                                          Aabb const&            box,
                                          LayoutMetrics const&   metrics);

    // True if `point` is within `thickness * 0.5` of the cubic bezier.
    // Sample-flattens to 32 segments and uses point-to-segment distance;
    // accurate enough for click selection at typical zoom levels.
    bool point_on_bezier(BezierPoints const& bez,
                         ImVec2 const&       point,
                         float               thickness);
}

#endif
