#include <memory>
#include <stdexcept>

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
using piper::engine::BuildDiagnostic;
using piper::engine::Engine;
using piper::engine::StepRegistry;

namespace build_diag_test
{
    bool has_kind(std::vector<BuildDiagnostic> const& diags,
                  BuildDiagnostic::Kind k)
    {
        for (auto const& d : diags)
        {
            if (d.kind == k)
            {
                return true;
            }
        }
        return false;
    }

    BuildDiagnostic const* find_kind(std::vector<BuildDiagnostic> const& diags,
                                     BuildDiagnostic::Kind k)
    {
        for (auto const& d : diags)
        {
            if (d.kind == k)
            {
                return &d;
            }
        }
        return nullptr;
    }

    // Not the engine's external IO step: external_io_kind() stays None.
    class FakeExternalInput final : public piper::engine::Step
    {
    public:
        void declare_io() override { declare_output<float>("out", out_); }
        void compute(piper::engine::Stage) override {}

    private:
        float out_{};
    };
}

using build_diag_test::find_kind;
using build_diag_test::has_kind;
using build_diag_test::FakeExternalInput;

TEST(EngineBuildDiagnostics, MissingInputNamesNodeAndPin)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto const* cf = nr.find("constant<float>");
    auto const* ad = nr.find("add<float>");

    auto c_id   = g.add_node(*cf, "src", "control", Point{});
    auto add_id = g.add_node(*ad, "sum", "control", Point{});
    g.add_link(PinRef{ c_id, "out" }, PinRef{ add_id, "a" }, "float");
    // "b" left unwired.

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    BuildDiagnostic const* d =
        find_kind(res.diagnostics, BuildDiagnostic::Kind::MissingInput);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->node_id,   add_id);
    EXPECT_EQ(d->attr_name, "b");
}

TEST(EngineBuildDiagnostics, OptionalInputUnwiredBuildsOk)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto const* cf  = nr.find("constant<float>");
    auto const* pid = nr.find("pid<float>");

    auto pid_id = g.add_node(*pid, "pid", "control", Point{});
    char const* required_pins[] = { "setpoint", "measured", "kp", "ki", "kd" };
    for (char const* pin : required_pins)
    {
        auto c = g.add_node(*cf, pin, "control", Point{});
        g.add_link(PinRef{ c, "out" }, PinRef{ pid_id, pin }, "float");
    }
    // Optional "dt_in" left unwired.

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_TRUE(res.ok);
    EXPECT_TRUE(res.diagnostics.empty());
}

TEST(EngineBuildDiagnostics, DuplicateInputWiringFails)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto const* cf = nr.find("constant<float>");
    auto const* ad = nr.find("add<float>");

    auto c1     = g.add_node(*cf, "c1",  "control", Point{});
    auto c2     = g.add_node(*cf, "c2",  "control", Point{});
    auto add_id = g.add_node(*ad, "sum", "control", Point{});
    g.add_link(PinRef{ c1, "out" }, PinRef{ add_id, "a" }, "float");
    g.add_link(PinRef{ c2, "out" }, PinRef{ add_id, "a" }, "float");
    g.add_link(PinRef{ c1, "out" }, PinRef{ add_id, "b" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    BuildDiagnostic const* d =
        find_kind(res.diagnostics, BuildDiagnostic::Kind::DuplicateInputWiring);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->node_id,   add_id);
    EXPECT_EQ(d->attr_name, "a");
}

TEST(EngineBuildDiagnostics, ThrowingFactoryEmitsStepConstructionFailed)
{
    piper::NodeType boom;
    boom.type = "boom";
    boom.attributes = {
        { "out", "float", piper::AttributeSpec::Role::Output, "" },
    };

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto boom_id = g.add_node(boom, "b", "control", Point{});

    StepRegistry sr;
    sr.add("boom", []() -> std::shared_ptr<piper::engine::Step>
    {
        throw std::runtime_error("kaboom");
    });

    Engine e;
    Engine::BuildResult res;
    EXPECT_NO_THROW(res = e.build(g, sr));
    EXPECT_FALSE(res.ok);
    BuildDiagnostic const* d =
        find_kind(res.diagnostics, BuildDiagnostic::Kind::StepConstructionFailed);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->node_id, boom_id);
}

TEST(EngineBuildDiagnostics, ForeignStepUnderExternalInputTypeEmitsFactoryTypeMismatch)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto const* ext = nr.find("external_input<float>");
    auto ext_id = g.add_node(*ext, "knob", "control", Point{});
    g.set_attr_value(ext_id, "name", "x");

    // Registered first, so register_builtin_steps' duplicate add for
    // the same type string is rejected and the foreign factory wins.
    StepRegistry sr;
    ASSERT_TRUE(sr.add("external_input<float>", []
    {
        return std::make_shared<FakeExternalInput>();
    }));
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    BuildDiagnostic const* d =
        find_kind(res.diagnostics, BuildDiagnostic::Kind::FactoryTypeMismatch);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->node_id, ext_id);
    // The foreign step must not be handed out as a typed handle.
    EXPECT_EQ(e.input<float>("x"), nullptr);
}

TEST(EngineBuildDiagnostics, ProbeFloatBuildsAndPlays)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto const* cf = nr.find("constant<float>");
    auto const* pb = nr.find("probe<float>");
    ASSERT_NE(pb, nullptr);

    auto c_id     = g.add_node(*cf, "src",   "control", Point{});
    auto probe_id = g.add_node(*pb, "probe", "control", Point{});
    g.set_attr_value(c_id, "value", "3.5");
    g.add_link(PinRef{ c_id, "out" }, PinRef{ probe_id, "in" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_TRUE(res.ok);
    EXPECT_TRUE(res.diagnostics.empty())
        << "probe<float> should have a registered factory";
    EXPECT_NE(e.step_for(probe_id), nullptr);
    EXPECT_NO_THROW(e.play());
}

TEST(EngineContract, TickBeforeBuildIsNoOp)
{
    Engine e;
    EXPECT_NO_THROW(e.tick("control"));
    EXPECT_NO_THROW(e.play());
    EXPECT_EQ(e.step_for(1), nullptr);
}

TEST(EngineContract, TickOnUnknownStageIsNoOp)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto const* cf = nr.find("constant<float>");
    auto const* eo = nr.find("external_output<float>");

    auto c_id = g.add_node(*cf, "src", "control", Point{});
    auto o_id = g.add_node(*eo, "out", "control", Point{});
    g.set_attr_value(c_id, "value", "7.0");
    g.set_attr_value(o_id, "name", "p");
    g.add_link(PinRef{ c_id, "out" }, PinRef{ o_id, "in" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok);

    auto const* probe = e.output<float>("p");
    ASSERT_NE(probe, nullptr);

    e.tick("ghost_stage");
    EXPECT_FLOAT_EQ(probe->get(), 0.0f);

    e.tick("control");
    EXPECT_FLOAT_EQ(probe->get(), 7.0f);
}

TEST(EngineContract, StepForUnknownIdReturnsNullptr)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto const* cf = nr.find("constant<float>");
    auto c_id = g.add_node(*cf, "src", "control", Point{});

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok);
    EXPECT_NE(e.step_for(c_id), nullptr);
    EXPECT_EQ(e.step_for(999999), nullptr);
}
