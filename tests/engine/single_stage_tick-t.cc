#include <cmath>

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
using piper::engine::step::Output;
using piper::engine::StepRegistry;

TEST(EngineTick, ConstantPropagatesThroughLowPass)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* cf = nr.find("constant<float>");
    auto const* lp = nr.find("low_pass<float>");
    auto const* pr = nr.find("external_output<float>");

    auto cf_id = g.add_node(*cf, "src",    "control", Point{ 0.0f, 0.0f });
    auto lp_id = g.add_node(*lp, "filter", "control", Point{ 1.0f, 0.0f });
    auto pr_id = g.add_node(*pr, "probe",  "control", Point{ 2.0f, 0.0f });

    g.set_attr_value(cf_id, "value",  "1.0");
    g.set_attr_value(lp_id, "cutoff", "10.0");

    g.add_link(PinRef{ cf_id, "out" }, PinRef{ lp_id, "in" }, "float");
    g.add_link(PinRef{ lp_id, "out" }, PinRef{ pr_id, "in" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok);

    for (int i = 0; i < 1000; ++i)
    {
        e.tick("control");
    }

    auto* probe = dynamic_cast<Output<float>*>(e.step_for(pr_id));
    ASSERT_NE(probe, nullptr);
    // Steady-state: low-pass with constant input converges to the input.
    EXPECT_NEAR(probe->get(), 1.0f, 1e-3f);
}

TEST(EngineTick, LabelClusterRoutesValueFromInToOut)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* cf = nr.find("constant<float>");
    auto const* pr = nr.find("external_output<float>");

    auto src_id   = g.add_node(*cf, "src",   "control", Point{ 0.0f, 0.0f });
    auto in_id    = g.add_label(piper::LabelKind::In,  "tap", Point{ 1.0f, 0.0f });
    auto out_id   = g.add_label(piper::LabelKind::Out, "tap", Point{ 2.0f, 0.0f });
    auto probe_id = g.add_node(*pr, "probe", "control", Point{ 3.0f, 0.0f });

    g.set_attr_value(src_id, "value", "7.0");
    g.add_link(PinRef{ src_id, "out" }, PinRef{ in_id,    piper::label_pin_name }, "float");
    g.add_link(PinRef{ out_id, piper::label_pin_name }, PinRef{ probe_id, "in" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok);

    e.tick("control");

    auto* probe = dynamic_cast<Output<float>*>(e.step_for(probe_id));
    ASSERT_NE(probe, nullptr);
    EXPECT_FLOAT_EQ(probe->get(), 7.0f);
}

TEST(EngineTick, LabelOutWithoutInFlagsDiagnostic)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* pr = nr.find("external_output<float>");

    auto out_id   = g.add_label(piper::LabelKind::Out, "tap", Point{ 0.0f, 0.0f });
    auto probe_id = g.add_node(*pr, "probe", "control", Point{ 1.0f, 0.0f });

    g.add_link(PinRef{ out_id, piper::label_pin_name }, PinRef{ probe_id, "in" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    bool found = false;
    for (auto const& d : res.diagnostics)
    {
        if (d.message.find("no label_in") != std::string::npos)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(EngineTick, LabelClusterTypeMismatchFlagsDiagnostic)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* cf     = nr.find("constant<float>");
    auto const* pr_int = nr.find("external_output<int32_t>");

    auto src_id   = g.add_node(*cf, "src",   "control", Point{ 0.0f, 0.0f });
    auto in_id    = g.add_label(piper::LabelKind::In,  "tap", Point{ 1.0f, 0.0f });
    auto out_id   = g.add_label(piper::LabelKind::Out, "tap", Point{ 2.0f, 0.0f });
    auto probe_id = g.add_node(*pr_int, "probe", "control", Point{ 3.0f, 0.0f });

    g.set_attr_value(src_id, "value", "5.0");
    g.add_link(PinRef{ src_id, "out" }, PinRef{ in_id,    piper::label_pin_name }, "float");
    g.add_link(PinRef{ out_id, piper::label_pin_name }, PinRef{ probe_id, "in" }, "int32_t");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    bool found = false;
    for (auto const& d : res.diagnostics)
    {
        if (d.kind == piper::engine::BuildDiagnostic::Kind::TypeMismatchAtLink)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(EngineTick, LabelInWithoutOutFlagsDiagnostic)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* cf = nr.find("constant<float>");

    auto src_id = g.add_node(*cf, "src", "control", Point{ 0.0f, 0.0f });
    auto in_id  = g.add_label(piper::LabelKind::In, "orphan", Point{ 1.0f, 0.0f });

    g.set_attr_value(src_id, "value", "1.0");
    g.add_link(PinRef{ src_id, "out" }, PinRef{ in_id, piper::label_pin_name }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    bool found = false;
    for (auto const& d : res.diagnostics)
    {
        if (d.message.find("no label_out") != std::string::npos)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(EngineTick, LabelClusterWithMultipleInsFlagsDiagnostic)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* cf = nr.find("constant<float>");

    auto src_a   = g.add_node(*cf, "srcA", "control", Point{ 0.0f, 0.0f });
    auto src_b   = g.add_node(*cf, "srcB", "control", Point{ 0.0f, 1.0f });
    auto in_a_id = g.add_label(piper::LabelKind::In, "tap", Point{ 1.0f, 0.0f });
    auto in_b_id = g.add_label(piper::LabelKind::In, "tap", Point{ 1.0f, 1.0f });

    g.set_attr_value(src_a,   "value", "1.0");
    g.set_attr_value(src_b,   "value", "2.0");
    g.set_attr_value(in_a_id, "name",  "tap");
    g.set_attr_value(in_b_id, "name",  "tap");
    g.add_link(PinRef{ src_a, "out" }, PinRef{ in_a_id, "in" }, "float");
    g.add_link(PinRef{ src_b, "out" }, PinRef{ in_b_id, "in" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    bool found = false;
    for (auto const& d : res.diagnostics)
    {
        if (d.message.find("multiple label_in") != std::string::npos)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}
