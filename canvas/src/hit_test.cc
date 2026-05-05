#include <algorithm>

#include "piper/canvas/hit_test.h"

namespace piper::canvas
{
    constexpr int bezier_samples = 32;

    ImVec2 bezier_at(BezierPoints const& bez, float t)
    {
        float const u  = 1.0f - t;
        float const b0 = u * u * u;
        float const b1 = 3.0f * u * u * t;
        float const b2 = 3.0f * u * t * t;
        float const b3 = t * t * t;
        return ImVec2{
            b0 * bez.a.x + b1 * bez.c1.x + b2 * bez.c2.x + b3 * bez.b.x,
            b0 * bez.a.y + b1 * bez.c1.y + b2 * bez.c2.y + b3 * bez.b.y,
        };
    }

    float point_to_segment_distance_sq(ImVec2 const& p, ImVec2 const& a, ImVec2 const& b)
    {
        float const abx = b.x - a.x;
        float const aby = b.y - a.y;
        float const apx = p.x - a.x;
        float const apy = p.y - a.y;
        float const len_sq = abx * abx + aby * aby;
        // Degenerate segment (a == b): distance from p to a.
        if (len_sq <= 0.0f)
        {
            return apx * apx + apy * apy;
        }
        float const t       = std::clamp((apx * abx + apy * aby) / len_sq, 0.0f, 1.0f);
        float const closex  = a.x + t * abx;
        float const closey  = a.y + t * aby;
        float const dx      = p.x - closex;
        float const dy      = p.y - closey;
        return dx * dx + dy * dy;
    }

    std::optional<NodeId> hit_test_node(std::span<Node const>  nodes,
                                        ImVec2 const&          point,
                                        LayoutMetrics const&   metrics)
    {
        // Reverse: later draw order = on top.
        for (std::size_t i = nodes.size(); i > 0; --i)
        {
            Node const& n = nodes[i - 1];
            if (node_aabb(n, metrics).contains(point))
            {
                return n.id;
            }
        }
        return std::nullopt;
    }

    std::vector<std::size_t> nodes_in_box(std::span<Node const>  nodes,
                                          Aabb const&            box,
                                          LayoutMetrics const&   metrics)
    {
        std::vector<std::size_t> result;
        result.reserve(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            if (node_aabb(nodes[i], metrics).intersects(box))
            {
                result.push_back(i);
            }
        }
        return result;
    }

    std::optional<PinHit> hit_test_pin(std::span<Node const>  nodes,
                                       ImVec2 const&          point,
                                       LayoutMetrics const&   metrics,
                                       float                  radius)
    {
        float const r_sq = radius * radius;
        for (std::size_t i = nodes.size(); i > 0; --i)
        {
            Node const& n = nodes[i - 1];
            for (std::size_t j = 0; j < n.outputs.size(); ++j)
            {
                ImVec2 const c  = pin_center_in_node(n, PinKind::Output, j, metrics);
                float const dx = point.x - c.x;
                float const dy = point.y - c.y;
                if (dx * dx + dy * dy <= r_sq)
                {
                    return PinHit{ n.id, &n.outputs[j], PinKind::Output, c };
                }
            }
            for (std::size_t j = 0; j < n.inputs.size(); ++j)
            {
                ImVec2 const c  = pin_center_in_node(n, PinKind::Input, j, metrics);
                float const dx = point.x - c.x;
                float const dy = point.y - c.y;
                if (dx * dx + dy * dy <= r_sq)
                {
                    return PinHit{ n.id, &n.inputs[j], PinKind::Input, c };
                }
            }
        }
        return std::nullopt;
    }

    bool point_on_bezier(BezierPoints const& bez,
                         ImVec2 const&       point,
                         float               thickness)
    {
        float const half      = thickness * 0.5f;
        float const threshold = half * half;

        ImVec2 prev = bez.a;
        for (int i = 1; i <= bezier_samples; ++i)
        {
            float const t     = float(i) / float(bezier_samples);
            ImVec2 const next = bezier_at(bez, t);
            if (point_to_segment_distance_sq(point, prev, next) <= threshold)
            {
                return true;
            }
            prev = next;
        }
        return false;
    }
}
