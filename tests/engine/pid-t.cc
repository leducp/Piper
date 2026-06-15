#include <gtest/gtest.h>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/registry.h"
#include "piper/stage.h"

#include "piper/engine/builtin_steps.h"
#include "piper/engine/engine.h"
#include "piper/engine/external_io.h"
#include "piper/engine/registry.h"

using piper::Graph;
using piper::NodeRegistry;
using piper::PinRef;
using piper::Point;
using piper::engine::Engine;
using piper::engine::StepRegistry;

namespace pid_test
{
    // pid<float> with setpoint/measured driven via external inputs and
    // gains wired to constants; members set from the test.
    struct Harness
    {
        NodeRegistry  nr;
        StepRegistry  sr;
        Graph         g;
        Engine        e;
        piper::NodeId pid_id{piper::invalid_node_id};

        piper::engine::step::Input<float>* setpoint{nullptr};
        piper::engine::step::Input<float>* measured{nullptr};

        bool build(char const* kp, char const* ki, char const* kd,
                   char const* dt, char const* out_min, char const* out_max,
                   char const* dt_in_value = nullptr)
        {
            piper::register_builtin_nodes(nr);
            piper::engine::register_builtin_steps(sr);
            g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

            auto const* pid_type = nr.find("pid<float>");
            auto const* cf       = nr.find("constant<float>");
            auto const* ext_in   = nr.find("external_input<float>");

            pid_id = g.add_node(*pid_type, "pid", "control", Point{});
            g.set_attr_value(pid_id, "dt",      dt);
            g.set_attr_value(pid_id, "out_min", out_min);
            g.set_attr_value(pid_id, "out_max", out_max);

            auto sp_id = g.add_node(*ext_in, "sp", "control", Point{});
            g.set_attr_value(sp_id, "name", "sp");
            g.add_link(PinRef{ sp_id, "out" }, PinRef{ pid_id, "setpoint" }, "float");

            auto m_id = g.add_node(*ext_in, "meas", "control", Point{});
            g.set_attr_value(m_id, "name", "meas");
            g.add_link(PinRef{ m_id, "out" }, PinRef{ pid_id, "measured" }, "float");

            auto wire_constant = [&](char const* pin, char const* value)
            {
                auto c = g.add_node(*cf, pin, "control", Point{});
                g.set_attr_value(c, "value", value);
                g.add_link(PinRef{ c, "out" }, PinRef{ pid_id, pin }, "float");
            };
            wire_constant("kp", kp);
            wire_constant("ki", ki);
            wire_constant("kd", kd);
            if (dt_in_value != nullptr)
            {
                wire_constant("dt_in", dt_in_value);
            }

            auto res = e.build(g, sr);
            if (not res.ok)
            {
                return false;
            }
            setpoint = e.input<float>("sp");
            measured = e.input<float>("meas");
            return setpoint != nullptr and measured != nullptr;
        }

        float out()
        {
            return e.step_for(pid_id)->output<float>("out");
        }
    };
}

using pid_test::Harness;

TEST(PidStep, PureProportional)
{
    Harness h;
    ASSERT_TRUE(h.build("2.5", "0.0", "0.0", "0.001", "-1e30", "1e30"));

    h.setpoint->set(1.0f);
    h.measured->set(0.0f);
    h.e.tick("control");
    EXPECT_FLOAT_EQ(h.out(), 2.5f);

    h.setpoint->set(0.4f);
    h.e.tick("control");
    EXPECT_FLOAT_EQ(h.out(), 1.0f);

    h.setpoint->set(-2.0f);
    h.e.tick("control");
    EXPECT_FLOAT_EQ(h.out(), -5.0f);
}

TEST(PidStep, OutputClampedToLimits)
{
    Harness h;
    ASSERT_TRUE(h.build("100.0", "0.0", "0.0", "0.001", "-1.5", "1.5"));

    h.setpoint->set(1.0f);
    h.measured->set(0.0f);
    h.e.tick("control");
    EXPECT_FLOAT_EQ(h.out(), 1.5f);

    h.setpoint->set(-1.0f);
    h.e.tick("control");
    EXPECT_FLOAT_EQ(h.out(), -1.5f);
}

TEST(PidStep, AntiWindupFreezesIntegralWhileSaturated)
{
    // Pure-I controller, dt = 0.1, output saturates at 0.5. With
    // conditional integration the integral stops near 0.5; without it
    // 30 ticks of e=1 would wind it up to ~3.0 and recovery would lag
    // for dozens of ticks.
    Harness h;
    ASSERT_TRUE(h.build("0.0", "1.0", "0.0", "0.1", "-0.5", "0.5"));

    h.setpoint->set(1.0f);
    h.measured->set(0.0f);
    for (int i = 0; i < 30; ++i)
    {
        h.e.tick("control");
    }
    EXPECT_FLOAT_EQ(h.out(), 0.5f);

    // Reverse: error becomes -0.3; the very next tick must drop below
    // the limit (integral was frozen, not wound up).
    h.setpoint->set(-0.3f);
    h.e.tick("control");
    EXPECT_LT(h.out(), 0.49f) << "recovery delayed: integral wound up while saturated";
    EXPECT_GT(h.out(), 0.3f);
}

TEST(PidStep, DtInOverridesDtMember)
{
    // Pure-I with dt member 0.001 but dt_in wired to 0.5: one tick of
    // e=1 integrates 0.5, so out = ki * 0.5. With the member dt the
    // result would be 0.001.
    Harness h;
    ASSERT_TRUE(h.build("0.0", "1.0", "0.0", "0.001", "-1e30", "1e30", "0.5"));

    h.setpoint->set(1.0f);
    h.measured->set(0.0f);
    h.e.tick("control");
    EXPECT_FLOAT_EQ(h.out(), 0.5f);
}
