#include <gtest/gtest.h>

#include "piper/canvas/cull.h"

using namespace piper::canvas;

TEST(LinkBezier, EndpointsArePassedThrough)
{
    auto const bez = link_bezier({ 100.0f, 50.0f }, { 300.0f, 75.0f }, 50.0f);
    EXPECT_FLOAT_EQ(bez.a.x, 100.0f);
    EXPECT_FLOAT_EQ(bez.a.y,  50.0f);
    EXPECT_FLOAT_EQ(bez.b.x, 300.0f);
    EXPECT_FLOAT_EQ(bez.b.y,  75.0f);
}

TEST(LinkBezier, ControlPointsExtendHorizontallyByDxOver2)
{
    // |dx| = 200, dx*0.5 = 100, base strength 50 → ext = 100.
    auto const bez = link_bezier({ 100.0f, 100.0f }, { 300.0f, 100.0f }, 50.0f);
    EXPECT_FLOAT_EQ(bez.c1.x, 200.0f);
    EXPECT_FLOAT_EQ(bez.c1.y, 100.0f);
    EXPECT_FLOAT_EQ(bez.c2.x, 200.0f);
    EXPECT_FLOAT_EQ(bez.c2.y, 100.0f);
}

TEST(LinkBezier, ShortDistanceFallsBackToMinStrength)
{
    // |dx| = 10, dx*0.5 = 5, base strength 50 → ext = 50.
    auto const bez = link_bezier({ 100.0f, 0.0f }, { 110.0f, 0.0f }, 50.0f);
    EXPECT_FLOAT_EQ(bez.c1.x, 150.0f);
    EXPECT_FLOAT_EQ(bez.c2.x,  60.0f);
}

TEST(LinkBezier, ReverseDirectionStillExtendsControlPointsAway)
{
    // Output to the right of input — bezier sweeps as an S-curve.
    auto const bez = link_bezier({ 300.0f, 100.0f }, { 100.0f, 100.0f }, 50.0f);
    // |dx| = 200, ext = 100. c1 leaves a to the right; c2 leaves b to the left.
    EXPECT_FLOAT_EQ(bez.c1.x, 400.0f);
    EXPECT_FLOAT_EQ(bez.c2.x,   0.0f);
}

TEST(LinkBezier, ControlPointsHaveSameYAsEndpoints)
{
    auto const bez = link_bezier({ 0.0f, 50.0f }, { 200.0f, 150.0f }, 50.0f);
    EXPECT_FLOAT_EQ(bez.c1.y,  50.0f);
    EXPECT_FLOAT_EQ(bez.c2.y, 150.0f);
}
