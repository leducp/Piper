#include <stdexcept>

#include <gtest/gtest.h>

#include "piper/vec.h"

using namespace piper;

TEST(ParseVec3f, ParsesPlainTriple)
{
    Vec3<float> const v = parse_vec3f("1,2,3");
    EXPECT_FLOAT_EQ(v.x, 1.0f);
    EXPECT_FLOAT_EQ(v.y, 2.0f);
    EXPECT_FLOAT_EQ(v.z, 3.0f);
}

TEST(ParseVec3f, ParsesNegativeAndFractional)
{
    Vec3<float> const v = parse_vec3f("-1.5,0.25,-3e2");
    EXPECT_FLOAT_EQ(v.x, -1.5f);
    EXPECT_FLOAT_EQ(v.y, 0.25f);
    EXPECT_FLOAT_EQ(v.z, -300.0f);
}

TEST(ParseVec3f, ToleratesWhitespace)
{
    Vec3<float> const v = parse_vec3f("  1.5 , -2.5 ,  3.25  ");
    EXPECT_FLOAT_EQ(v.x, 1.5f);
    EXPECT_FLOAT_EQ(v.y, -2.5f);
    EXPECT_FLOAT_EQ(v.z, 3.25f);
}

TEST(ParseVec3f, ThrowsOnTwoComponents)
{
    EXPECT_THROW(parse_vec3f("1,2"), std::runtime_error);
}

TEST(ParseVec3f, ThrowsOnGarbage)
{
    EXPECT_THROW(parse_vec3f("abc"), std::invalid_argument);
    EXPECT_THROW(parse_vec3f(""), std::invalid_argument);
}

TEST(ParseVec3f, ThrowsOnWrongSeparator)
{
    EXPECT_THROW(parse_vec3f("1;2;3"), std::runtime_error);
}
