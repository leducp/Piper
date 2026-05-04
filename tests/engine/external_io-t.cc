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
using piper::engine::step::Input;
using piper::engine::step::Output;
using piper::engine::StepRegistry;

namespace piper_engine_test
{
    Graph build_external_passthrough()
    {
        NodeRegistry nr;
        piper::register_builtin_nodes(nr);

        Graph g;
        g.add_stage(piper::Stage{ "control" });

        auto const* in_t  = nr.find("external_input<float>");
        auto const* out_t = nr.find("external_output<float>");
        auto const* lp_t  = nr.find("low_pass<float>");

        auto in_id  = g.add_node(*in_t,  "ipc_in",  "control", Point{ 0.0f, 0.0f });
        auto lp_id  = g.add_node(*lp_t,  "filter",  "control", Point{ 1.0f, 0.0f });
        auto out_id = g.add_node(*out_t, "ipc_out", "control", Point{ 2.0f, 0.0f });
        (void) in_id; (void) out_id;

        g.set_attr_value(in_id,  "name",   "target");
        g.set_attr_value(lp_id,  "cutoff", "100.0");
        g.set_attr_value(out_id, "name",   "measured");

        g.add_link(PinRef{ in_id,  "out" }, PinRef{ lp_id,  "in" }, "float");
        g.add_link(PinRef{ lp_id,  "out" }, PinRef{ out_id, "in" }, "float");

        return g;
    }
}

TEST(EngineExternalIO, SetInputFlowsThroughPipelineToOutput)
{
    Graph g = piper_engine_test::build_external_passthrough();

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok) << (res.diagnostics.empty() ? "" : res.diagnostics.front().message);

    auto* target = e.input<float>("target");
    auto const* measured = e.output<float>("measured");
    ASSERT_NE(target, nullptr);
    ASSERT_NE(measured, nullptr);

    target->set(1.0f);
    for (int i = 0; i < 1000; ++i)
    {
        e.play();
    }
    EXPECT_NEAR(measured->get(), 1.0f, 1e-3f);

    target->set(-2.5f);
    for (int i = 0; i < 1000; ++i)
    {
        e.play();
    }
    EXPECT_NEAR(measured->get(), -2.5f, 1e-3f);
}

TEST(EngineExternalIO, UnknownNameReturnsNullptr)
{
    Graph g = piper_engine_test::build_external_passthrough();
    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);
    Engine e;
    ASSERT_TRUE(e.build(g, sr).ok);

    EXPECT_EQ(e.input<float>("does_not_exist"),  nullptr);
    EXPECT_EQ(e.output<float>("does_not_exist"), nullptr);
    EXPECT_EQ(e.input<int32_t>("target"),            nullptr);   // wrong type
}

TEST(EngineExternalIO, EmptyNameSkipsHalIndexButStillTicks)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control" });

    auto const* cf    = nr.find("constant<float>");
    auto const* out_t = nr.find("external_output<float>");
    auto src_id  = g.add_node(*cf,    "src",  "control", Point{ 0.0f, 0.0f });
    auto sink_id = g.add_node(*out_t, "sink", "control", Point{ 1.0f, 0.0f });
    g.set_attr_value(src_id, "value", "2.0");
    // Intentionally do NOT set "name" on the sink: the node still
    // ticks, but it is not reachable via Engine::output<T>(name).
    g.add_link(PinRef{ src_id, "out" }, PinRef{ sink_id, "in" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok) << (res.diagnostics.empty() ? "" : res.diagnostics.front().message);

    e.tick("control");

    EXPECT_EQ(e.output<float>("anything"), nullptr);

    auto const* by_id = e.step_for(sink_id);
    ASSERT_NE(by_id, nullptr);
    EXPECT_FLOAT_EQ(by_id->input<float>("in"), 2.0f);
}
