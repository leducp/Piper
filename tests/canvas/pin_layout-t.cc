#include <gtest/gtest.h>

#include <vector>

#include "piper/canvas/cull.h"
#include "piper/canvas/graph.h"

using namespace piper::canvas;

Pin make_pin(uint64_t id)
{
    Pin p{};
    p.id = PinId{id};
    return p;
}

TEST(PinLayout, FirstInputCenterIsAtNodeLeftEdge)
{
    LayoutMetrics const m;
    std::vector<Pin> ins{ make_pin(1) };

    Node n{};
    n.pos    = { 100.0f, 200.0f };
    n.inputs = std::span<Pin const>(ins);

    ImVec2 const c = pin_center_in_node(n, PinKind::Input, 0, m);
    EXPECT_FLOAT_EQ(c.x, n.pos.x);
    EXPECT_FLOAT_EQ(c.y, n.pos.y + m.header_height + 0.5f * m.pin_row_height);
}

TEST(PinLayout, FirstOutputCenterIsAtNodeRightEdge)
{
    LayoutMetrics const m;
    std::vector<Pin> outs{ make_pin(1) };

    Node n{};
    n.pos     = { 100.0f, 200.0f };
    n.outputs = std::span<Pin const>(outs);

    ImVec2 const c = pin_center_in_node(n, PinKind::Output, 0, m);
    EXPECT_FLOAT_EQ(c.x, n.pos.x + m.min_width);
    EXPECT_FLOAT_EQ(c.y, n.pos.y + m.header_height + 0.5f * m.pin_row_height);
}

TEST(PinLayout, MultiplePinsStackVerticallyByRowHeight)
{
    LayoutMetrics const m;
    std::vector<Pin> ins{ make_pin(1), make_pin(2), make_pin(3) };

    Node n{};
    n.pos    = { 0.0f, 0.0f };
    n.inputs = std::span<Pin const>(ins);

    ImVec2 const c0 = pin_center_in_node(n, PinKind::Input, 0, m);
    ImVec2 const c1 = pin_center_in_node(n, PinKind::Input, 1, m);
    ImVec2 const c2 = pin_center_in_node(n, PinKind::Input, 2, m);

    EXPECT_FLOAT_EQ(c1.y - c0.y, m.pin_row_height);
    EXPECT_FLOAT_EQ(c2.y - c1.y, m.pin_row_height);
    EXPECT_FLOAT_EQ(c0.x, c1.x);
    EXPECT_FLOAT_EQ(c1.x, c2.x);
}

TEST(PinLayout, BodyMinSizeWidensOutputPinPosition)
{
    LayoutMetrics const m;
    std::vector<Pin> outs{ make_pin(1) };

    Node n{};
    n.pos           = { 0.0f, 0.0f };
    n.body_min_size = { 300.0f, 0.0f };   // wider than min_width
    n.outputs       = std::span<Pin const>(outs);

    ImVec2 const c = pin_center_in_node(n, PinKind::Output, 0, m);
    EXPECT_FLOAT_EQ(c.x, 300.0f);
}

TEST(PinLayout, FlippedInputRendersOnRightEdge)
{
    LayoutMetrics const m;
    std::vector<Pin> ins{ make_pin(1) };
    ins[0].flip_side = true;

    Node n{};
    n.pos    = { 100.0f, 200.0f };
    n.inputs = std::span<Pin const>(ins);

    ImVec2 const c = pin_center_in_node(n, PinKind::Input, 0, m);
    EXPECT_FLOAT_EQ(c.x, n.pos.x + m.min_width);   // right edge, not left
    EXPECT_FLOAT_EQ(c.y, n.pos.y + m.header_height + 0.5f * m.pin_row_height);
}

TEST(PinLayout, FlippedOutputRendersOnLeftEdge)
{
    LayoutMetrics const m;
    std::vector<Pin> outs{ make_pin(1) };
    outs[0].flip_side = true;

    Node n{};
    n.pos     = { 100.0f, 200.0f };
    n.outputs = std::span<Pin const>(outs);

    ImVec2 const c = pin_center_in_node(n, PinKind::Output, 0, m);
    EXPECT_FLOAT_EQ(c.x, n.pos.x);   // left edge
}

TEST(PinLayout, FlipReflowsRowsPerSideColumn)
{
    LayoutMetrics const m;
    // Two inputs; flip the first to the right. Left column then holds
    // only input[1] at row 0; right column holds input[0] at row 0.
    std::vector<Pin> ins{ make_pin(1), make_pin(2) };
    ins[0].flip_side = true;

    Node n{};
    n.pos    = { 0.0f, 0.0f };
    n.inputs = std::span<Pin const>(ins);

    ImVec2 const c_flipped = pin_center_in_node(n, PinKind::Input, 0, m);
    ImVec2 const c_left    = pin_center_in_node(n, PinKind::Input, 1, m);

    float const row0_y = m.header_height + 0.5f * m.pin_row_height;
    EXPECT_FLOAT_EQ(c_flipped.x, n.pos.x + m.min_width);
    EXPECT_FLOAT_EQ(c_flipped.y, row0_y);   // row 0 of the right column
    EXPECT_FLOAT_EQ(c_left.x, n.pos.x);
    EXPECT_FLOAT_EQ(c_left.y, row0_y);       // row 0 of the left column
}

TEST(PinLayout, PinOnLeftHelperMatchesFlip)
{
    EXPECT_TRUE(pin_on_left(PinKind::Input,  false));
    EXPECT_FALSE(pin_on_left(PinKind::Input,  true));
    EXPECT_FALSE(pin_on_left(PinKind::Output, false));
    EXPECT_TRUE(pin_on_left(PinKind::Output, true));
}
