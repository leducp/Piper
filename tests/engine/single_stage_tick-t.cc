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
