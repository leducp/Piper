#include <gtest/gtest.h>

#include "piper/canvas/transform.h"

using namespace piper::canvas;

TEST(Transform, IdentityCanvasOriginMapsToScreenOrigin)
{
    Transform t;
    auto s = t.to_screen({0.0f, 0.0f}, {100.0f, 50.0f});
    EXPECT_FLOAT_EQ(s.x, 100.0f);
    EXPECT_FLOAT_EQ(s.y,  50.0f);
}

TEST(Transform, IdentityRoundTrip)
{
    Transform t;
    ImVec2 const origin{100.0f, 50.0f};
    ImVec2 const pt{37.0f, 19.0f};

    auto round = t.to_canvas(t.to_screen(pt, origin), origin);
    EXPECT_FLOAT_EQ(round.x, pt.x);
    EXPECT_FLOAT_EQ(round.y, pt.y);
}

TEST(Transform, PanShiftsViewWithoutScaling)
{
    Transform t;
    t.pan = {10.0f, 20.0f};

    // Canvas point at the pan position lives at the screen origin.
    auto s = t.to_screen({10.0f, 20.0f}, {0.0f, 0.0f});
    EXPECT_FLOAT_EQ(s.x, 0.0f);
    EXPECT_FLOAT_EQ(s.y, 0.0f);
}

TEST(Transform, ZoomScalesAroundPan)
{
    Transform t;
    t.zoom = 2.0f;

    auto s = t.to_screen({10.0f, 5.0f}, {0.0f, 0.0f});
    EXPECT_FLOAT_EQ(s.x, 20.0f);
    EXPECT_FLOAT_EQ(s.y, 10.0f);
}

TEST(Transform, PanAndZoomCompose)
{
    Transform t;
    t.pan  = {5.0f, 10.0f};
    t.zoom = 2.0f;

    // Canvas (5, 10) is at the pan, so it maps to the screen origin.
    auto s_at_pan = t.to_screen({5.0f, 10.0f}, {100.0f, 200.0f});
    EXPECT_FLOAT_EQ(s_at_pan.x, 100.0f);
    EXPECT_FLOAT_EQ(s_at_pan.y, 200.0f);

    // (15, 30) is +10 in x and +20 in y from pan, scaled by 2:
    // +20 px and +40 px from screen origin.
    auto s_offset = t.to_screen({15.0f, 30.0f}, {100.0f, 200.0f});
    EXPECT_FLOAT_EQ(s_offset.x, 120.0f);
    EXPECT_FLOAT_EQ(s_offset.y, 240.0f);
}

TEST(Transform, RoundTripAtVariousScales)
{
    for (float zoom : { 0.25f, 0.5f, 1.0f, 2.0f, 5.0f })
    {
        Transform t;
        t.zoom = zoom;
        t.pan  = { 17.0f, 23.0f };

        ImVec2 const origin{55.0f, 88.0f};
        ImVec2 const pt{42.0f, 13.0f};

        auto round = t.to_canvas(t.to_screen(pt, origin), origin);
        EXPECT_NEAR(round.x, pt.x, 1e-4f);
        EXPECT_NEAR(round.y, pt.y, 1e-4f);
    }
}

TEST(Transform, ScreenToCanvasIsInverseOfCanvasToScreen)
{
    Transform t;
    t.pan  = { 7.5f, -13.25f };
    t.zoom = 1.5f;
    ImVec2 const origin{ 200.0f, 300.0f };

    for (ImVec2 pt : { ImVec2{0.0f,  0.0f},
                       ImVec2{1.0f,  1.0f},
                       ImVec2{-50.0f, 75.0f},
                       ImVec2{1000.0f, -1000.0f} })
    {
        auto round = t.to_canvas(t.to_screen(pt, origin), origin);
        EXPECT_NEAR(round.x, pt.x, 1e-3f);
        EXPECT_NEAR(round.y, pt.y, 1e-3f);
    }
}

TEST(Transform, ZoomAroundCursorIdiomKeepsPointStable)
{
    // The Editor uses this pattern: capture canvas-pos at cursor
    // before zoom change, change zoom, then adjust pan so the same
    // canvas-pos still maps to the cursor.
    Transform t;
    ImVec2 const origin{ 100.0f, 100.0f };
    ImVec2 const cursor{ 250.0f, 180.0f };

    ImVec2 const before = t.to_canvas(cursor, origin);
    t.zoom = 2.5f;
    ImVec2 const after  = t.to_canvas(cursor, origin);
    t.pan.x += before.x - after.x;
    t.pan.y += before.y - after.y;

    // After the adjustment, cursor should map to the same canvas-pos.
    ImVec2 const final_pt = t.to_canvas(cursor, origin);
    EXPECT_NEAR(final_pt.x, before.x, 1e-4f);
    EXPECT_NEAR(final_pt.y, before.y, 1e-4f);
}
