#include <stdint.h>

#include <gtest/gtest.h>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/registry.h"
#include "piper/stage.h"

#include "piper/vec.h"

#include "piper/engine/builtin_steps.h"
#include "piper/engine/engine.h"
#include "piper/engine/external_io.h"
#include "piper/engine/registry.h"

using piper::Graph;
using piper::NodeRegistry;
using piper::PinRef;
using piper::Point;
using piper::Vec3;
using piper::engine::Engine;
using piper::engine::StepRegistry;

namespace kernels_test
{
    struct Rig
    {
        NodeRegistry nr;
        StepRegistry sr;
        Graph        g;
        Engine       e;

        Rig()
        {
            piper::register_builtin_nodes(nr);
            piper::engine::register_builtin_steps(sr);
            g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
        }

        piper::NodeId constant(char const* type, char const* value)
        {
            auto const* nt = nr.find(type);
            auto id = g.add_node(*nt, "c", "control", Point{});
            g.set_attr_value(id, "value", value);
            return id;
        }

        piper::NodeId node(char const* type, char const* name)
        {
            auto const* nt = nr.find(type);
            return g.add_node(*nt, name, "control", Point{});
        }

        bool build_ok()
        {
            return e.build(g, sr).ok;
        }
    };
}

using kernels_test::Rig;

TEST(Kernels, AddFloat)
{
    Rig r;
    auto a   = r.constant("constant<float>", "2.5");
    auto b   = r.constant("constant<float>", "4.25");
    auto sum = r.node("add<float>", "sum");
    r.g.add_link(PinRef{ a, "out" }, PinRef{ sum, "a" }, "float");
    r.g.add_link(PinRef{ b, "out" }, PinRef{ sum, "b" }, "float");

    ASSERT_TRUE(r.build_ok());
    r.e.tick("control");
    EXPECT_FLOAT_EQ(r.e.step_for(sum)->output<float>("out"), 6.75f);
}

TEST(Kernels, SubtractFloat)
{
    Rig r;
    auto a    = r.constant("constant<float>", "5.0");
    auto b    = r.constant("constant<float>", "7.5");
    auto diff = r.node("subtract<float>", "diff");
    r.g.add_link(PinRef{ a, "out" }, PinRef{ diff, "a" }, "float");
    r.g.add_link(PinRef{ b, "out" }, PinRef{ diff, "b" }, "float");

    ASSERT_TRUE(r.build_ok());
    r.e.tick("control");
    EXPECT_FLOAT_EQ(r.e.step_for(diff)->output<float>("out"), -2.5f);
}

TEST(Kernels, MultiplyFloat)
{
    Rig r;
    auto a    = r.constant("constant<float>", "3.0");
    auto b    = r.constant("constant<float>", "-4.5");
    auto prod = r.node("multiply<float>", "prod");
    r.g.add_link(PinRef{ a, "out" }, PinRef{ prod, "a" }, "float");
    r.g.add_link(PinRef{ b, "out" }, PinRef{ prod, "b" }, "float");

    ASSERT_TRUE(r.build_ok());
    r.e.tick("control");
    EXPECT_FLOAT_EQ(r.e.step_for(prod)->output<float>("out"), -13.5f);
}

TEST(Kernels, AbsFloatNegativeInput)
{
    Rig r;
    auto c = r.constant("constant<float>", "-3.5");
    auto a = r.node("abs<float>", "abs");
    r.g.add_link(PinRef{ c, "out" }, PinRef{ a, "in" }, "float");

    ASSERT_TRUE(r.build_ok());
    r.e.tick("control");
    EXPECT_FLOAT_EQ(r.e.step_for(a)->output<float>("out"), 3.5f);
}

TEST(Kernels, AbsInt32NegativeInput)
{
    Rig r;
    auto c = r.constant("constant<int32_t>", "-7");
    auto a = r.node("abs<int32_t>", "abs");
    r.g.add_link(PinRef{ c, "out" }, PinRef{ a, "in" }, "int32_t");

    ASSERT_TRUE(r.build_ok());
    r.e.tick("control");
    EXPECT_EQ(r.e.step_for(a)->output<int32_t>("out"), 7);
}

TEST(Kernels, ClampBelowInsideAbove)
{
    Rig r;
    auto drive = r.node("external_input<float>", "drive");
    r.g.set_attr_value(drive, "name", "drive");
    auto cl = r.node("clamp<float>", "clamp");
    r.g.set_attr_value(cl, "min", "-1.0");
    r.g.set_attr_value(cl, "max", "1.0");
    r.g.add_link(PinRef{ drive, "out" }, PinRef{ cl, "in" }, "float");

    ASSERT_TRUE(r.build_ok());
    auto* in = r.e.input<float>("drive");
    ASSERT_NE(in, nullptr);

    in->set(-5.0f);
    r.e.tick("control");
    EXPECT_FLOAT_EQ(r.e.step_for(cl)->output<float>("out"), -1.0f);

    in->set(0.25f);
    r.e.tick("control");
    EXPECT_FLOAT_EQ(r.e.step_for(cl)->output<float>("out"), 0.25f);

    in->set(9.0f);
    r.e.tick("control");
    EXPECT_FLOAT_EQ(r.e.step_for(cl)->output<float>("out"), 1.0f);
}

TEST(Kernels, CastFloatToInt32TruncatesTowardZero)
{
    Rig r;
    auto pos      = r.constant("constant<float>", "2.9");
    auto neg      = r.constant("constant<float>", "-2.9");
    auto cast_pos = r.node("cast<int32_t>", "cast_pos");
    auto cast_neg = r.node("cast<int32_t>", "cast_neg");
    r.g.add_link(PinRef{ pos, "out" }, PinRef{ cast_pos, "in" }, "float");
    r.g.add_link(PinRef{ neg, "out" }, PinRef{ cast_neg, "in" }, "float");

    ASSERT_TRUE(r.build_ok());
    r.e.tick("control");
    // static_cast<int32_t>(float) truncates toward zero.
    EXPECT_EQ(r.e.step_for(cast_pos)->output<int32_t>("out"),  2);
    EXPECT_EQ(r.e.step_for(cast_neg)->output<int32_t>("out"), -2);
}

TEST(Kernels, Mux3SelectsAndSaturates)
{
    Rig r;
    auto in0 = r.constant("constant<float>", "10.0");
    auto in1 = r.constant("constant<float>", "20.0");
    auto in2 = r.constant("constant<float>", "30.0");
    auto sel = r.node("external_input<int32_t>", "sel");
    r.g.set_attr_value(sel, "name", "sel");
    auto mux = r.node("mux3<float>", "mux");
    r.g.add_link(PinRef{ in0, "out" }, PinRef{ mux, "in0" },    "float");
    r.g.add_link(PinRef{ in1, "out" }, PinRef{ mux, "in1" },    "float");
    r.g.add_link(PinRef{ in2, "out" }, PinRef{ mux, "in2" },    "float");
    r.g.add_link(PinRef{ sel, "out" }, PinRef{ mux, "select" }, "int32_t");

    ASSERT_TRUE(r.build_ok());
    auto* select = r.e.input<int32_t>("sel");
    ASSERT_NE(select, nullptr);

    auto mux_out = [&]
    {
        r.e.tick("control");
        return r.e.step_for(mux)->output<float>("out");
    };

    select->set(0);
    EXPECT_FLOAT_EQ(mux_out(), 10.0f);
    select->set(1);
    EXPECT_FLOAT_EQ(mux_out(), 20.0f);
    select->set(2);
    EXPECT_FLOAT_EQ(mux_out(), 30.0f);
    // Out-of-range selectors saturate to in0 / in2.
    select->set(-5);
    EXPECT_FLOAT_EQ(mux_out(), 10.0f);
    select->set(99);
    EXPECT_FLOAT_EQ(mux_out(), 30.0f);
}

TEST(Kernels, Vec3ConstantComponents)
{
    Rig r;
    auto a   = r.constant("constant<vec3<float>>", "1,2,3");
    auto b   = r.constant("constant<vec3<float>>", "10,20,30");
    auto sum = r.node("add<vec3<float>>", "sum");
    r.g.add_link(PinRef{ a, "out" }, PinRef{ sum, "a" }, "vec3<float>");
    r.g.add_link(PinRef{ b, "out" }, PinRef{ sum, "b" }, "vec3<float>");

    ASSERT_TRUE(r.build_ok());
    r.e.tick("control");

    Vec3<float> const src = r.e.step_for(a)->output<Vec3<float>>("out");
    EXPECT_FLOAT_EQ(src.x, 1.0f);
    EXPECT_FLOAT_EQ(src.y, 2.0f);
    EXPECT_FLOAT_EQ(src.z, 3.0f);

    Vec3<float> const v = r.e.step_for(sum)->output<Vec3<float>>("out");
    EXPECT_FLOAT_EQ(v.x, 11.0f);
    EXPECT_FLOAT_EQ(v.y, 22.0f);
    EXPECT_FLOAT_EQ(v.z, 33.0f);
}
