#include <gtest/gtest.h>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/registry.h"
#include "piper/stage.h"

#include "piper/engine/builtin_steps.h"
#include "piper/engine/engine.h"
#include "piper/engine/registry.h"

using piper::AttributeSpec;
using piper::Graph;
using piper::Node;
using piper::NodeRegistry;
using piper::PinRef;
using piper::Point;
using piper::engine::BuildDiagnostic;
using piper::engine::Engine;
using piper::engine::StepRegistry;

namespace piper_engine_test
{
    Graph make_linear_chain()
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

        g.set_attr_value(cf_id, "value", "1.0");
        g.set_attr_value(lp_id, "cutoff", "10.0");

        g.add_link(PinRef{ cf_id, "out" }, PinRef{ lp_id, "in" }, "float");
        g.add_link(PinRef{ lp_id, "out" }, PinRef{ pr_id, "in" }, "float");
        return g;
    }
}

TEST(EngineBuild, BuildSucceedsForLinearGraph)
{
    Graph g = piper_engine_test::make_linear_chain();

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);

    ASSERT_TRUE(res.ok) << (res.diagnostics.empty()
                                ? std::string{"no diagnostics"}
                                : res.diagnostics.front().message);
    EXPECT_TRUE(res.diagnostics.empty());
    EXPECT_EQ(e.stages().size(), 1u);
}

TEST(EngineBuild, UnwiredRequiredInputFailsBuild)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* lp = nr.find("low_pass<float>");
    auto const* pr = nr.find("external_output<float>");
    auto lp_id = g.add_node(*lp, "filter", "control", Point{ 0.0f, 0.0f });
    auto pr_id = g.add_node(*pr, "probe",  "control", Point{ 1.0f, 0.0f });

    // Wire only the output; low_pass "in" (required) is left unwired.
    // "dt_in" is also unwired but optional, so it must not be flagged.
    g.add_link(PinRef{ lp_id, "out" }, PinRef{ pr_id, "in" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);

    EXPECT_FALSE(res.ok);
    bool flagged_in    = false;
    bool flagged_dt_in = false;
    for (auto const& d : res.diagnostics)
    {
        if (d.kind == BuildDiagnostic::Kind::UnresolvedInput)
        {
            if (d.attr_name == "in")    { flagged_in = true; }
            if (d.attr_name == "dt_in") { flagged_dt_in = true; }
        }
    }
    EXPECT_TRUE(flagged_in)      << "required 'in' should fail the build";
    EXPECT_FALSE(flagged_dt_in)  << "optional 'dt_in' must not fail the build";
}

TEST(EngineName, DefaultsEmptyAndSurvivesBuild)
{
    Graph g = piper_engine_test::make_linear_chain();

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    EXPECT_TRUE(e.name().empty());

    e.set_name("motor_loop");
    EXPECT_EQ(e.name(), "motor_loop");

    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok);
    EXPECT_EQ(e.name(), "motor_loop");
}
