#include <gtest/gtest.h>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/registry.h"
#include "piper/stage.h"

#include "piper/engine/builtin_steps.h"
#include "piper/engine/engine.h"
#include "piper/engine/registry.h"

using piper::Graph;
using piper::NodeRegistry;
using piper::Point;
using piper::engine::BuildDiagnostic;
using piper::engine::Engine;
using piper::engine::StepRegistry;

TEST(EngineBuild, MalformedMemberValueEmitsStepDeclareIoFailed)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* cf = nr.find("constant<float>");
    auto cf_id = g.add_node(*cf, "src", "control", Point{ 0.0f, 0.0f });
    g.set_attr_value(cf_id, "value", "not-a-number");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);

    EXPECT_FALSE(res.ok);
    bool found = false;
    for (auto const& d : res.diagnostics)
    {
        if (d.kind == BuildDiagnostic::Kind::StepDeclareIoFailed)
        {
            found = true;
            EXPECT_EQ(d.node_id, cf_id);
        }
    }
    EXPECT_TRUE(found);
}
