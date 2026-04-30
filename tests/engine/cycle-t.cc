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
using piper::engine::BuildDiagnosticKind;
using piper::engine::Engine;
using piper::engine::StepRegistry;

TEST(EngineBuild, CycleIsDiagnosedWithOffendingLink)
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

    LinkId const link_ab = g.add_link(PinRef{ a_id, "out" }, PinRef{ b_id, "in" }, "float");
    LinkId const link_ba = g.add_link(PinRef{ b_id, "out" }, PinRef{ a_id, "in" }, "float");
    ASSERT_NE(link_ab, piper::invalid_link_id);
    ASSERT_NE(link_ba, piper::invalid_link_id);

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);

    EXPECT_FALSE(res.ok);
    bool found_cycle = false;
    for (auto const& d : res.diagnostics)
    {
        if (d.kind == BuildDiagnosticKind::CycleDetected)
        {
            found_cycle = true;
            EXPECT_NE(d.link_id, piper::invalid_link_id);
            EXPECT_TRUE(d.link_id == link_ab or d.link_id == link_ba);
        }
    }
    EXPECT_TRUE(found_cycle);
}

TEST(EngineBuild, SelfLoopIsRejectedAsCycle)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* lp = nr.find("low_pass<float>");
    auto a_id = g.add_node(*lp, "a", "control", Point{ 0.0f, 0.0f });
    g.set_attr_value(a_id, "cutoff", "10.0");

    LinkId const self = g.add_link(PinRef{ a_id, "out" }, PinRef{ a_id, "in" }, "float");
    ASSERT_NE(self, piper::invalid_link_id);

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);

    EXPECT_FALSE(res.ok);
    bool found_cycle = false;
    for (auto const& d : res.diagnostics)
    {
        if (d.kind == BuildDiagnosticKind::CycleDetected)
        {
            found_cycle = true;
            EXPECT_EQ(d.link_id, self);
        }
    }
    EXPECT_TRUE(found_cycle);
}
