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
            declare_output<int32_t>("out", out_);
        }

        void compute(piper::engine::Slot) override
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
            { "out", "int32_t", AttributeSpec::Role::Output, "" },
        };
        nt.slots = { "control", "feedback" };
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
    probe_meta.type       = "external_output<int32_t>";
    probe_meta.attributes = {
        { "in", "int32_t", AttributeSpec::Role::Input, "" },
    };
    probe_meta.slots = { "tick" };

    auto counter_id = g.add_node(counter,    "ctr",   Point{ 0.0f, 0.0f });
    auto probe_id   = g.add_node(probe_meta, "probe", Point{ 1.0f, 0.0f });

    g.bind_slot(counter_id, "control",  "control");
    g.bind_slot(counter_id, "feedback", "feedback");
    g.bind_slot(probe_id,   "tick",     "feedback");

    g.add_link(PinRef{ counter_id, "out" }, PinRef{ probe_id, "in" }, "int32_t");

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
    auto* probe = dynamic_cast<Output<int32_t>*>(e.step_for(probe_id));
    ASSERT_NE(ctr,   nullptr);
    ASSERT_NE(probe, nullptr);

    EXPECT_EQ(ctr->ticks(),  6);
    EXPECT_EQ(probe->get(), 6);
}
