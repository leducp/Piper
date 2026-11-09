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
        g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

        auto const* in_t  = nr.find("external_input<float>");
        auto const* out_t = nr.find("external_output<float>");
        auto const* lp_t  = nr.find("low_pass<float>");

        auto in_id  = g.add_node(*in_t,  "ipc_in",  "control", Point{ 0.0f, 0.0f });
        auto lp_id  = g.add_node(*lp_t,  "filter",  "control", Point{ 1.0f, 0.0f });
        auto out_id = g.add_node(*out_t, "ipc_out", "control", Point{ 2.0f, 0.0f });
        (void) in_id; (void) out_id;

        g.set_node_slot(in_id, "target");
        g.set_attr_value(lp_id,  "cutoff", "100.0");
        g.set_node_slot(out_id, "measured");

        g.add_link(PinRef{ in_id,  "out" }, PinRef{ lp_id,  "in" }, "float");
        g.add_link(PinRef{ lp_id,  "out" }, PinRef{ out_id, "in" }, "float");

        return g;
    }

    Graph build_external_passthrough_double()
    {
        NodeRegistry nr;
        piper::register_builtin_nodes(nr);

        Graph g;
        g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

        auto const* in_t  = nr.find("external_input<double>");
        auto const* out_t = nr.find("external_output<double>");
        auto const* lp_t  = nr.find("low_pass<double>");

        auto in_id  = g.add_node(*in_t,  "ipc_in",  "control", Point{ 0.0f, 0.0f });
        auto lp_id  = g.add_node(*lp_t,  "filter",  "control", Point{ 1.0f, 0.0f });
        auto out_id = g.add_node(*out_t, "ipc_out", "control", Point{ 2.0f, 0.0f });
        (void) in_id; (void) out_id;

        g.set_node_slot(in_id, "target");
        g.set_attr_value(lp_id,  "cutoff", "100.0");
        g.set_node_slot(out_id, "measured");

        g.add_link(PinRef{ in_id,  "out" }, PinRef{ lp_id,  "in" }, "double");
        g.add_link(PinRef{ lp_id,  "out" }, PinRef{ out_id, "in" }, "double");

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

TEST(EngineExternalIO, EmptySlotSkipsHalIndexButStillTicks)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* cf    = nr.find("constant<float>");
    auto const* out_t = nr.find("external_output<float>");
    auto src_id  = g.add_node(*cf,    "src",  "control", Point{ 0.0f, 0.0f });
    auto sink_id = g.add_node(*out_t, "sink", "control", Point{ 1.0f, 0.0f });
    g.set_attr_value(src_id, "value", "2.0");
    // Intentionally do NOT set a slot on the sink: the node still
    // ticks, but it is not reachable via Engine::output<T>(slot).
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

TEST(EngineExternalIO, DoubleSetInputFlowsThroughPipelineToOutput)
{
    Graph g = piper_engine_test::build_external_passthrough_double();

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok) << (res.diagnostics.empty() ? "" : res.diagnostics.front().message);

    auto* target = e.input<double>("target");
    auto const* measured = e.output<double>("measured");
    ASSERT_NE(target, nullptr);
    ASSERT_NE(measured, nullptr);

    target->set(1.0);
    for (int i = 0; i < 1000; ++i)
    {
        e.play();
    }
    EXPECT_NEAR(measured->get(), 1.0, 1e-3);

    target->set(-2.5);
    for (int i = 0; i < 1000; ++i)
    {
        e.play();
    }
    EXPECT_NEAR(measured->get(), -2.5, 1e-3);
}

TEST(EngineExternalIO, DoubleUnknownNameReturnsNullptr)
{
    Graph g = piper_engine_test::build_external_passthrough_double();
    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);
    Engine e;
    ASSERT_TRUE(e.build(g, sr).ok);

    EXPECT_EQ(e.input<double>("does_not_exist"),  nullptr);
    EXPECT_EQ(e.output<double>("does_not_exist"), nullptr);
    EXPECT_EQ(e.input<float>("target"),           nullptr);   // wrong type
}

TEST(EngineExternalIO, DoubleDuplicateExternalSlotFailsBuild)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* in_t = nr.find("external_input<double>");
    auto a_id = g.add_node(*in_t, "in_a", "control", Point{ 0.0f, 0.0f });
    auto b_id = g.add_node(*in_t, "in_b", "control", Point{ 1.0f, 0.0f });
    // Two external inputs sharing the same I/O slot -> the
    // host index cannot disambiguate them, so build() must reject it.
    g.set_node_slot(a_id, "target");
    g.set_node_slot(b_id, "target");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);

    bool found_duplicate = false;
    for (auto const& d : res.diagnostics)
    {
        if (d.kind == piper::engine::BuildDiagnostic::Kind::UnresolvedInput
            and d.message.find("duplicate external_input<double>") != std::string::npos)
        {
            found_duplicate = true;
        }
    }
    EXPECT_TRUE(found_duplicate);
}

TEST(EngineExternalIO, SlotsRemainStableAcrossModesAndDisplayRenames)
{
    NodeRegistry nodes;
    piper::register_builtin_nodes(nodes);
    Graph graph;
    graph.add_stage(piper::Stage{"control", 0xFFFFFFFFu});
    auto input_id =
        graph.add_node(*nodes.find("external_input<float>"), "Input title", "control", {});
    auto output_id =
        graph.add_node(*nodes.find("external_output<float>"), "Output title", "control", {});
    ASSERT_TRUE(graph.set_node_slot(input_id, "target"));
    ASSERT_TRUE(graph.set_node_slot(output_id, "measured"));
    graph.add_link({input_id, "out"}, {output_id, "in"}, "float");
    graph.add_mode_profile({"first", {{input_id, "enable"}, {output_id, "enable"}}});
    graph.add_mode_profile({"second", {{input_id, "track"}, {output_id, "track"}}});
    graph.set_default_mode_name("first");
    StepRegistry registry;
    piper::engine::register_builtin_steps(registry);
    Engine engine;
    ASSERT_TRUE(engine.build(graph, registry).ok);
    auto* input = engine.input<float>("target");
    auto const* output = engine.output<float>("measured");
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);
    EXPECT_EQ(engine.input<float>("Input title"), nullptr);
    input->set(42);
    engine.set_mode("second");
    EXPECT_EQ(engine.input<float>("target"), input);
    EXPECT_EQ(engine.output<float>("measured"), output);
    engine.play();
    EXPECT_FLOAT_EQ(output->get(), 42);
    graph.rename_node(input_id, "Renamed input");
    graph.set_node_slot(input_id, "new_target");
    EXPECT_EQ(engine.input<float>("target"), input);
    ASSERT_TRUE(engine.build(graph, registry).ok);
    EXPECT_EQ(engine.input<float>("target"), nullptr);
    EXPECT_NE(engine.input<float>("new_target"), nullptr);
    EXPECT_EQ(engine.input<float>("Renamed input"), nullptr);
}
