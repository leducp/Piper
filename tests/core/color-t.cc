#include <gtest/gtest.h>

#include "piper/color.h"

using namespace piper;

TEST(PastelFromHueIndex, IsDeterministic)
{
    rgba const a = pastel_from_hue_index(7);
    rgba const b = pastel_from_hue_index(7);
    EXPECT_EQ(a, b);
}

TEST(PastelFromHueIndex, DifferentIndicesProduceDifferentColors)
{
    rgba const a = pastel_from_hue_index(0);
    rgba const b = pastel_from_hue_index(1);
    rgba const c = pastel_from_hue_index(2);
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);
}

TEST(PastelFromHueIndex, FullSaturationAndValueWhenOpaque)
{
    rgba const c = pastel_from_hue_index(0);
    // Alpha is always opaque.
    EXPECT_EQ(c.a(), 0xFFu);
}

TEST(PastelFromHueIndex, RespectsSaturationAndValueArgs)
{
    // saturation=0 → grayscale of `value`. r == g == b.
    rgba const gray = pastel_from_hue_index(3, 0.0f, 0.5f);
    EXPECT_EQ(gray.r(), gray.g());
    EXPECT_EQ(gray.g(), gray.b());
    // value=0.5 → channels around 128.
    EXPECT_NEAR(gray.r(), 128, 2);
}

TEST(PastelFromHueIndex, NegativeIndexStillProducesValidColor)
{
    rgba const c = pastel_from_hue_index(-3);
    EXPECT_EQ(c.a(), 0xFFu);
}
