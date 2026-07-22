#include <gtest/gtest.h>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/link.h"
#include "piper/registry.h"
#include "piper/stage.h"

#include "piper/engine/builtin_steps.h"
#include "piper/engine/engine.h"
#include "piper/engine/registry.h"

using piper::Graph;
using piper::LinkId;
using piper::NodeRegistry;
using piper::PinRef;
using piper::Point;
using piper::engine::BuildDiagnostic;
using piper::engine::Engine;
using piper::engine::StepRegistry;

namespace
{
    bool has_kind(piper::engine::Engine::BuildResult const& res,
                  BuildDiagnostic::Kind kind)
    {
        for (auto const& d : res.diagnostics)
        {
            if (d.kind == kind)
            {
                return true;
            }
        }
        return false;
    }
}

// Probe (a): a genuine two-node same-stage cycle whose edge's pin carries a
// TYPO'd stage name. The typo makes pin_active false on every stage, dropping
// the edge from all ordering graphs and (pre-fix) silently hiding the cycle so
// build() returned ok. An undeclared pin stage is a config error -> fail build.
TEST(PinStageTypoCycle, TypoedPinStageBreaksCycleButFailsBuild)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* lp = nr.find("low_pass<float>");
    auto a_id = g.add_node(*lp, "a", "control", Point{ 0.0f, 0.0f });
    auto b_id = g.add_node(*lp, "b", "control", Point{ 1.0f, 0.0f });
    g.set_attr_value(a_id, "cutoff", "10.0");
    g.set_attr_value(b_id, "cutoff", "10.0");

    g.add_link(PinRef{ a_id, "out" }, PinRef{ b_id, "in" }, "float");
    g.add_link(PinRef{ b_id, "out" }, PinRef{ a_id, "in" }, "float");

    // Typo on the back-link's producer pin: "cntrol" is not a declared stage.
    ASSERT_TRUE(g.set_attr_stages(b_id, "out",
                                  std::vector<std::string>{ "cntrol" }));

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);

    EXPECT_FALSE(res.ok);
    EXPECT_TRUE(has_kind(res, BuildDiagnostic::Kind::UnknownStageOnPin));
}

// Probe (b): a stage is removed AFTER a pin was staged on it. remove_stage does
// not cascade, so the pin's stage list survives pointing at a stage the graph
// no longer owns -- same fail-open primitive as a typo.
TEST(PinStageTypoCycle, StageRemovedUnderStagedPinFailsBuild)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    g.add_stage(piper::Stage{ "extra",   0xFFFFFFFFu });

    auto const* lp = nr.find("low_pass<float>");
    auto a_id = g.add_node(*lp, "a", "control", Point{ 0.0f, 0.0f });
    auto b_id = g.add_node(*lp, "b", "control", Point{ 1.0f, 0.0f });
    g.set_attr_value(a_id, "cutoff", "10.0");
    g.set_attr_value(b_id, "cutoff", "10.0");

    g.add_link(PinRef{ a_id, "out" }, PinRef{ b_id, "in" }, "float");
    g.add_link(PinRef{ b_id, "out" }, PinRef{ a_id, "in" }, "float");

    ASSERT_TRUE(g.set_attr_stages(b_id, "out",
                                  std::vector<std::string>{ "extra" }));
    g.remove_stage("extra");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);

    EXPECT_FALSE(res.ok);
    EXPECT_TRUE(has_kind(res, BuildDiagnostic::Kind::UnknownStageOnPin));
}

// Probe (c): a node whose HOME stage is typo'd closes a cycle through its
// default (unstaged) pins. Default pins inherit the home stage, so the typo
// resolves them to nothing and hides the cycle -- the home-stage absorb must
// itself be fatal.
TEST(PinStageTypoCycle, TypoedHomeStageThroughDefaultPinFailsBuild)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* lp = nr.find("low_pass<float>");
    // "cntrol" is a typo of the only declared stage.
    auto a_id = g.add_node(*lp, "a", "cntrol", Point{ 0.0f, 0.0f });
    auto b_id = g.add_node(*lp, "b", "control", Point{ 1.0f, 0.0f });
    g.set_attr_value(a_id, "cutoff", "10.0");
    g.set_attr_value(b_id, "cutoff", "10.0");

    g.add_link(PinRef{ a_id, "out" }, PinRef{ b_id, "in" }, "float");
    g.add_link(PinRef{ b_id, "out" }, PinRef{ a_id, "in" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);

    EXPECT_FALSE(res.ok);
    EXPECT_TRUE(has_kind(res, BuildDiagnostic::Kind::UnknownStageOnPin));
}

// Guard: with CORRECT stage names the same two-node same-stage cycle must still
// be caught as a real cycle, not masked by the fatal-typo change.
TEST(PinStageTypoCycle, CorrectStageNamesStillDetectCycle)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* lp = nr.find("low_pass<float>");
    auto a_id = g.add_node(*lp, "a", "control", Point{ 0.0f, 0.0f });
    auto b_id = g.add_node(*lp, "b", "control", Point{ 1.0f, 0.0f });
    g.set_attr_value(a_id, "cutoff", "10.0");
    g.set_attr_value(b_id, "cutoff", "10.0");

    g.add_link(PinRef{ a_id, "out" }, PinRef{ b_id, "in" }, "float");
    g.add_link(PinRef{ b_id, "out" }, PinRef{ a_id, "in" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);

    EXPECT_FALSE(res.ok);
    EXPECT_TRUE(has_kind(res, BuildDiagnostic::Kind::CycleDetected));
    EXPECT_FALSE(has_kind(res, BuildDiagnostic::Kind::UnknownStageOnPin));
}
