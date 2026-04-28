#include <gtest/gtest.h>

#include "piper/canvas/aabb.h"

using namespace piper::canvas;

TEST(Aabb, IntersectsOverlapping)
{
    Aabb const a{ { 0.0f, 0.0f },  { 10.0f, 10.0f } };
    Aabb const b{ { 5.0f, 5.0f },  { 15.0f, 15.0f } };
    EXPECT_TRUE(a.intersects(b));
    EXPECT_TRUE(b.intersects(a));
}

TEST(Aabb, IntersectsContained)
{
    Aabb const outer{ { 0.0f,  0.0f  }, { 100.0f, 100.0f } };
    Aabb const inner{ { 25.0f, 25.0f }, {  75.0f,  75.0f } };
    EXPECT_TRUE(outer.intersects(inner));
    EXPECT_TRUE(inner.intersects(outer));
}

TEST(Aabb, IntersectsTouchingEdges)
{
    Aabb const a{ {  0.0f, 0.0f }, { 10.0f, 10.0f } };
    Aabb const b{ { 10.0f, 0.0f }, { 20.0f, 10.0f } };
    EXPECT_TRUE(a.intersects(b));
}

TEST(Aabb, DisjointHorizontally)
{
    Aabb const a{ {  0.0f, 0.0f }, { 10.0f, 10.0f } };
    Aabb const b{ { 20.0f, 0.0f }, { 30.0f, 10.0f } };
    EXPECT_FALSE(a.intersects(b));
}

TEST(Aabb, DisjointVertically)
{
    Aabb const a{ { 0.0f,  0.0f }, { 10.0f, 10.0f } };
    Aabb const b{ { 0.0f, 20.0f }, { 10.0f, 30.0f } };
    EXPECT_FALSE(a.intersects(b));
}

TEST(Aabb, ContainsInterior)
{
    Aabb const a{ { 0.0f, 0.0f }, { 100.0f, 100.0f } };
    EXPECT_TRUE(a.contains({ 50.0f, 50.0f }));
}

TEST(Aabb, ContainsOnEdge)
{
    Aabb const a{ { 0.0f, 0.0f }, { 100.0f, 100.0f } };
    EXPECT_TRUE(a.contains({   0.0f,   0.0f }));
    EXPECT_TRUE(a.contains({ 100.0f, 100.0f }));
    EXPECT_TRUE(a.contains({  50.0f,   0.0f }));
}

TEST(Aabb, RejectsPointsOutside)
{
    Aabb const a{ { 0.0f, 0.0f }, { 100.0f, 100.0f } };
    EXPECT_FALSE(a.contains({ -1.0f,  50.0f }));
    EXPECT_FALSE(a.contains({ 101.0f, 50.0f }));
    EXPECT_FALSE(a.contains({  50.0f, -1.0f }));
    EXPECT_FALSE(a.contains({  50.0f, 101.0f }));
}
