#include <gtest/gtest.h>

#include <vector>

#include "piper/canvas/cull.h"
#include "piper/canvas/graph.h"

using namespace piper::canvas;

namespace
{
    Node make_node(float x, float y)
    {
        Node n{};
        n.id  = NodeId{1};
        n.pos = { x, y };
        return n;
    }
}

TEST(NodeAabb, NodeWithoutPinsHasMinimumSize)
{
    LayoutMetrics const metrics;
    Node const n = make_node(10.0f, 20.0f);

    Aabb const box = node_aabb(n, metrics);
    EXPECT_FLOAT_EQ(box.min.x, 10.0f);
    EXPECT_FLOAT_EQ(box.min.y, 20.0f);
    EXPECT_FLOAT_EQ(box.max.x, 10.0f + metrics.min_width);
    EXPECT_FLOAT_EQ(box.max.y, 20.0f + metrics.header_height + metrics.min_body_height);
}

TEST(NodeAabb, BodyMinSizeOverridesDefaults)
{
    LayoutMetrics const metrics;
    Node n = make_node(0.0f, 0.0f);
    n.body_min_size = { 200.0f, 100.0f };

    Aabb const box = node_aabb(n, metrics);
    EXPECT_FLOAT_EQ(box.max.x, 200.0f);
    EXPECT_FLOAT_EQ(box.max.y, metrics.header_height + 100.0f);
}

TEST(CullVisible, IncludesNodesInsideViewport)
{
    LayoutMetrics const metrics;
    std::vector<Node> nodes{ make_node(0.0f, 0.0f), make_node(50.0f, 50.0f) };
    Aabb const viewport{ { -100.0f, -100.0f }, { 200.0f, 200.0f } };

    auto const visible = cull_visible(nodes, viewport, metrics);
    EXPECT_EQ(visible.size(), 2u);
}

TEST(CullVisible, ExcludesNodesOutsideViewport)
{
    LayoutMetrics const metrics;
    std::vector<Node> nodes{
        make_node(0.0f,    0.0f),
        make_node(1000.0f, 0.0f),
        make_node(0.0f,    1000.0f),
    };
    Aabb const viewport{ { -100.0f, -100.0f }, { 200.0f, 200.0f } };

    auto const visible = cull_visible(nodes, viewport, metrics);
    ASSERT_EQ(visible.size(), 1u);
    EXPECT_EQ(visible[0], 0u);
}

TEST(CullVisible, IncludesPartiallyOverlappingNodes)
{
    LayoutMetrics const metrics;
    // Node positioned so its right edge crosses the viewport's left
    // edge by 1 px.
    std::vector<Node> nodes{ make_node(99.0f, 0.0f) };
    Aabb const viewport{ { 200.0f, 0.0f }, { 400.0f, 100.0f } };
    // node.max.x = 99 + 120 = 219, viewport.min.x = 200 → intersects.

    auto const visible = cull_visible(nodes, viewport, metrics);
    EXPECT_EQ(visible.size(), 1u);
}

TEST(CullVisible, EmptyInputProducesEmptyOutput)
{
    LayoutMetrics const metrics;
    std::vector<Node> nodes;
    Aabb const viewport{ { 0.0f, 0.0f }, { 100.0f, 100.0f } };

    auto const visible = cull_visible(nodes, viewport, metrics);
    EXPECT_TRUE(visible.empty());
}

TEST(CullVisible, AllNodesCulled)
{
    LayoutMetrics const metrics;
    std::vector<Node> nodes{
        make_node(1000.0f, 1000.0f),
        make_node(2000.0f, 2000.0f),
    };
    Aabb const viewport{ { 0.0f, 0.0f }, { 100.0f, 100.0f } };

    auto const visible = cull_visible(nodes, viewport, metrics);
    EXPECT_TRUE(visible.empty());
}
