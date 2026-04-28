#include <gtest/gtest.h>

#include <vector>

#include "piper/canvas/cull.h"
#include "piper/canvas/graph.h"
#include "piper/canvas/hit_test.h"

using namespace piper::canvas;

Node make_node(uint64_t id, float x, float y)
{
    Node n{};
    n.id  = NodeId{id};
    n.pos = { x, y };
    return n;
}

TEST(HitTestNode, MissReturnsNullopt)
{
    LayoutMetrics const m;
    std::vector<Node> nodes{ make_node(1, 0.0f, 0.0f) };

    auto hit = hit_test_node(nodes, ImVec2{ -100.0f, -100.0f }, m);
    EXPECT_FALSE(hit.has_value());
}

TEST(HitTestNode, PointInsideAabbHits)
{
    LayoutMetrics const m;
    std::vector<Node> nodes{ make_node(7, 100.0f, 100.0f) };

    auto hit = hit_test_node(nodes, ImVec2{ 110.0f, 110.0f }, m);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, NodeId{7});
}

TEST(HitTestNode, TopmostWinsOnOverlap)
{
    LayoutMetrics const m;
    // Both nodes at the same origin — second is drawn on top.
    std::vector<Node> nodes{
        make_node(1, 0.0f, 0.0f),
        make_node(2, 0.0f, 0.0f),
    };

    auto hit = hit_test_node(nodes, ImVec2{ 5.0f, 5.0f }, m);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(*hit, NodeId{2});
}

TEST(NodesInBox, EmptyInputProducesEmpty)
{
    LayoutMetrics const m;
    std::vector<Node> nodes;
    Aabb const box{ { 0.0f, 0.0f }, { 100.0f, 100.0f } };

    auto const r = nodes_in_box(nodes, box, m);
    EXPECT_TRUE(r.empty());
}

TEST(NodesInBox, ReturnsIntersectingIndices)
{
    LayoutMetrics const m;
    std::vector<Node> nodes{
        make_node(1, 0.0f,   0.0f),
        make_node(2, 500.0f, 0.0f),
        make_node(3, 50.0f,  20.0f),
    };
    Aabb const box{ { -10.0f, -10.0f }, { 200.0f, 200.0f } };

    auto const r = nodes_in_box(nodes, box, m);
    ASSERT_EQ(r.size(), 2u);
    EXPECT_EQ(r[0], 0u);
    EXPECT_EQ(r[1], 2u);
}

TEST(PointOnBezier, PointAtEndpointHits)
{
    BezierPoints const bez{
        ImVec2{ 0.0f, 0.0f },
        ImVec2{ 50.0f, 0.0f },
        ImVec2{ 50.0f, 100.0f },
        ImVec2{ 100.0f, 100.0f },
    };
    EXPECT_TRUE(point_on_bezier(bez, ImVec2{ 0.0f, 0.0f }, 2.0f));
    EXPECT_TRUE(point_on_bezier(bez, ImVec2{ 100.0f, 100.0f }, 2.0f));
}

TEST(PointOnBezier, FarPointMisses)
{
    BezierPoints const bez{
        ImVec2{ 0.0f, 0.0f },
        ImVec2{ 50.0f, 0.0f },
        ImVec2{ 50.0f, 100.0f },
        ImVec2{ 100.0f, 100.0f },
    };
    EXPECT_FALSE(point_on_bezier(bez, ImVec2{ 500.0f, 500.0f }, 4.0f));
}

TEST(PointOnBezier, StraightLineMidpointHits)
{
    // Degenerate cubic bezier with control points on the line a→b →
    // the curve reduces to that line. Midpoint must be on the curve.
    BezierPoints const bez{
        ImVec2{ 0.0f, 0.0f },
        ImVec2{ 33.0f, 0.0f },
        ImVec2{ 66.0f, 0.0f },
        ImVec2{ 100.0f, 0.0f },
    };
    EXPECT_TRUE(point_on_bezier(bez, ImVec2{ 50.0f, 0.0f }, 1.0f));
    EXPECT_TRUE(point_on_bezier(bez, ImVec2{ 50.0f, 0.4f }, 1.0f));
    EXPECT_FALSE(point_on_bezier(bez, ImVec2{ 50.0f, 5.0f }, 1.0f));
}

TEST(PointOnBezier, ThickerThresholdAcceptsFurtherPoints)
{
    BezierPoints const bez{
        ImVec2{ 0.0f, 0.0f },
        ImVec2{ 33.0f, 0.0f },
        ImVec2{ 66.0f, 0.0f },
        ImVec2{ 100.0f, 0.0f },
    };
    ImVec2 const probe{ 50.0f, 3.0f };
    EXPECT_FALSE(point_on_bezier(bez, probe, 2.0f));   // half-thick = 1
    EXPECT_TRUE(point_on_bezier(bez, probe, 8.0f));    // half-thick = 4
}

Pin make_pin(uint64_t id, PinKind kind)
{
    Pin p{};
    p.id   = PinId{id};
    p.kind = kind;
    return p;
}

Node make_node_with_pins(uint64_t id, ImVec2 pos,
                         std::span<Pin const> ins,
                         std::span<Pin const> outs)
{
    Node n{};
    n.id      = NodeId{id};
    n.pos     = pos;
    n.inputs  = ins;
    n.outputs = outs;
    return n;
}

TEST(HitTestPin, FindsInputAtNodeLeftEdge)
{
    LayoutMetrics const m;
    std::vector<Pin> ins{ make_pin(1, PinKind::Input) };
    std::vector<Pin> outs;
    std::vector<Node> nodes{ make_node_with_pins(1, { 100.0f, 100.0f }, ins, outs) };

    ImVec2 const center{ 100.0f, 100.0f + m.header_height + 0.5f * m.pin_row_height };
    auto const hit = hit_test_pin(nodes, center, m, 4.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->pin->id, PinId{1});
    EXPECT_EQ(hit->kind,    PinKind::Input);
    EXPECT_EQ(hit->node_id, NodeId{1});
}

TEST(HitTestPin, FindsOutputAtNodeRightEdge)
{
    LayoutMetrics const m;
    std::vector<Pin> ins;
    std::vector<Pin> outs{ make_pin(7, PinKind::Output) };
    std::vector<Node> nodes{ make_node_with_pins(2, { 0.0f, 0.0f }, ins, outs) };

    ImVec2 const center{ m.min_width, m.header_height + 0.5f * m.pin_row_height };
    auto const hit = hit_test_pin(nodes, center, m, 4.0f);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->pin->id, PinId{7});
    EXPECT_EQ(hit->kind,    PinKind::Output);
}

TEST(HitTestPin, MissBetweenPinsReturnsNullopt)
{
    LayoutMetrics const m;
    std::vector<Pin> ins{ make_pin(1, PinKind::Input) };
    std::vector<Pin> outs;
    std::vector<Node> nodes{ make_node_with_pins(1, { 0.0f, 0.0f }, ins, outs) };

    ImVec2 const far_off{ 50.0f, 50.0f };
    auto const hit = hit_test_pin(nodes, far_off, m, 4.0f);
    EXPECT_FALSE(hit.has_value());
}

TEST(HitTestPin, RadiusControlsForgiveness)
{
    LayoutMetrics const m;
    std::vector<Pin> ins{ make_pin(1, PinKind::Input) };
    std::vector<Pin> outs;
    std::vector<Node> nodes{ make_node_with_pins(1, { 0.0f, 0.0f }, ins, outs) };

    ImVec2 const center{ 0.0f, m.header_height + 0.5f * m.pin_row_height };
    ImVec2 const offset{ center.x + 5.0f, center.y };

    EXPECT_FALSE(hit_test_pin(nodes, offset, m, 4.0f).has_value());
    EXPECT_TRUE (hit_test_pin(nodes, offset, m, 8.0f).has_value());
}
