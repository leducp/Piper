#include <gtest/gtest.h>

#include "piper/attribute.h"
#include "piper/graph.h"
#include "piper/node_type.h"
#include "piper/stage.h"

#include "piper/engine/builtin_steps.h"
#include "piper/engine/engine.h"
#include "piper/engine/external_io.h"
#include "piper/engine/registry.h"
#include "piper/engine/step.h"

using piper::AttributeSpec;
using piper::Graph;
using piper::NodeType;
using piper::PinRef;
using piper::Point;
using piper::engine::Engine;
using piper::engine::step::Output;
using piper::engine::Step;
using piper::engine::StepRegistry;

namespace piper_engine_test
{
    class CounterStep final : public Step
    {
    public:
        void declare_io() override
        {
            declare_output<int>("out", out_);
        }

        void compute(piper::engine::Stage) override
        {
            ++ticks_;
            out_ = ticks_;
        }

        int ticks() const { return ticks_; }
        int value() const { return out_;   }

    private:
        int out_{0};
        int ticks_{0};
    };

    NodeType make_counter_meta()
    {
        NodeType nt;
        nt.type     = "test_counter";
        nt.category = "test";
        nt.attributes = {
            { "out", "int", AttributeSpec::Role::Output, "" },
        };
        return nt;
    }
}

TEST(EngineTick, PerPinStagesActivateStepInExtraStage)
{
    Graph g;
    g.add_stage(piper::Stage{ "control",  0xFFFFFFFFu });
    g.add_stage(piper::Stage{ "feedback", 0xFFFFFFFFu });

    NodeType const counter = piper_engine_test::make_counter_meta();
    NodeType probe_meta;
    probe_meta.type     = "external_output<int>";
    probe_meta.attributes = {
        { "in", "int", AttributeSpec::Role::Input, "" },
    };

    auto counter_id = g.add_node(counter,    "ctr",   "control",  Point{ 0.0f, 0.0f });
    auto probe_id   = g.add_node(probe_meta, "probe", "feedback", Point{ 1.0f, 0.0f });

    // Per-pin stage override: counter's "out" is active in BOTH stages,
    // so the counter step ticks during "control" and "feedback".
    ASSERT_TRUE(g.set_attr_stages(counter_id, "out",
                                   std::vector<std::string>{ "control", "feedback" }));

    g.add_link(PinRef{ counter_id, "out" }, PinRef{ probe_id, "in" }, "int");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);
    sr.add("test_counter", []
    {
        return std::make_shared<piper_engine_test::CounterStep>();
    });

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok) << (res.diagnostics.empty() ? "" : res.diagnostics.front().message);

    for (int i = 0; i < 5; ++i)
    {
        e.tick("control");
    }
    e.tick("feedback");

    auto* ctr   = dynamic_cast<piper_engine_test::CounterStep*>(e.step_for(counter_id));
    auto* probe = dynamic_cast<Output<int>*>(e.step_for(probe_id));
    ASSERT_NE(ctr,   nullptr);
    ASSERT_NE(probe, nullptr);

    EXPECT_EQ(ctr->ticks(),  6);
    EXPECT_EQ(probe->get(), 6);
}
