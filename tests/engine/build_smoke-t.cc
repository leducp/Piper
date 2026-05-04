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
        g.add_stage(piper::Stage{ "control" });

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
