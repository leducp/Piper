#include <gtest/gtest.h>

#include <memory>

#include "piper/attribute.h"
#include "piper/graph.h"
#include "piper/node_type.h"
#include "piper/stage.h"

#include "piper/engine/engine.h"
#include "piper/engine/registry.h"
#include "piper/engine/step.h"

using piper::AttributeSpec;
using piper::Graph;
using piper::NodeType;
using piper::PinRef;
using piper::Point;
using piper::engine::Engine;
using piper::engine::hash_name;
using piper::engine::Step;
using piper::engine::StepRegistry;

namespace piper_engine_test
{
    // A hardware-singleton-style device step: ONE live instance shared by
    // several engines (e.g. a drive slave commanded by its joint pipeline
    // and observed by a monitor pipeline). The registry factory returns
    // the same instance every time -- recreating hardware is conceptually
    // wrong. This only works because the engine rebinds the step's
    // IoBlock before every compute(): each engine keeps its own wiring,
    // while caller-owned output slots make the state shared.
    class SharedDeviceStep final : public Step
    {
    public:
        static constexpr auto READ  = hash_name("read");
        static constexpr auto WRITE = hash_name("write");

        void declare_io() override
        {
            declare_output<double>("measured", measured_);
            // Optional: the monitor pipeline wires only "measured" and
            // leaves "command" unwired; build() must not flag it.
            declare_input<double>("command", /*optional=*/true);
        }

        void compute(piper::engine::Stage current) override
        {
            if (current.id == READ)
            {
                measured_ = state_;
            }
            else if (current.id == WRITE and has_input("command"))
            {
                state_ = input<double>("command");
            }
        }

        double state() const { return state_; }

    private:
        double state_{0.0};
        double measured_{0.0};
    };

    class SourceStep final : public Step
    {
    public:
        void declare_io() override { declare_output<double>("out", value_); }
        void compute(piper::engine::Stage) override {}
        void set(double v) { value_ = v; }

    private:
        double value_{0.0};
    };

    class SinkStep final : public Step
    {
    public:
        void declare_io() override { declare_input<double>("in"); }
        void compute(piper::engine::Stage) override { last_ = input<double>("in"); }
        double last() const { return last_; }

    private:
        double last_{0.0};
    };

    NodeType device_meta()
    {
        NodeType nt;
        nt.type       = "shared_device";
        nt.category   = "test";
        nt.attributes = {
            { "measured", "double", AttributeSpec::Role::Output, "" },
            { "command",  "double", AttributeSpec::Role::Input,  "", false, true },
        };
        return nt;
    }

    NodeType source_meta()
    {
        NodeType nt;
        nt.type       = "test_source";
        nt.category   = "test";
        nt.attributes = { { "out", "double", AttributeSpec::Role::Output, "" } };
        return nt;
    }

    NodeType sink_meta()
    {
        NodeType nt;
        nt.type       = "test_sink";
        nt.category   = "test";
        nt.attributes = { { "in", "double", AttributeSpec::Role::Input, "" } };
        return nt;
    }
}

// One live device instance in TWO engines: a control pipeline commands it,
// a monitor pipeline observes it. Both builds must succeed (the old
// init-throws-once guard forbade this), both wirings must resolve against
// the ticking engine, and the state must be shared through the single
// instance.
TEST(EngineSharedStep, OneLiveInstanceServesTwoEngines)
{
    using namespace piper_engine_test;

    auto device = std::make_shared<SharedDeviceStep>();

    StepRegistry sr;
    sr.add("shared_device", [device] { return device; });
    sr.add("test_source",   [] { return std::make_shared<SourceStep>(); });
    sr.add("test_sink",     [] { return std::make_shared<SinkStep>(); });

    // Control pipeline: source(control) -> device.command[write].
    Graph control;
    control.add_stage(piper::Stage{ "read",    0xFFFFFFFFu });
    control.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    control.add_stage(piper::Stage{ "write",   0xFFFFFFFFu });
    auto src_id  = control.add_node(source_meta(), "source", "control", Point{ 0.f, 0.f });
    auto dev_a   = control.add_node(device_meta(), "device", "read",    Point{ 1.f, 0.f });
    ASSERT_TRUE(control.set_attr_stages(dev_a, "command", { "write" }));
    control.add_link(PinRef{ src_id, "out" }, PinRef{ dev_a, "command" }, "double");

    // Monitor pipeline: device.measured[read] -> sink(control).
    Graph monitor;
    monitor.add_stage(piper::Stage{ "read",    0xFFFFFFFFu });
    monitor.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto dev_b  = monitor.add_node(device_meta(), "device", "read",    Point{ 0.f, 1.f });
    auto snk_id = monitor.add_node(sink_meta(),   "sink",   "control", Point{ 1.f, 1.f });
    monitor.add_link(PinRef{ dev_b, "measured" }, PinRef{ snk_id, "in" }, "double");

    Engine control_engine;
    auto res_a = control_engine.build(control, sr);
    ASSERT_TRUE(res_a.ok) << (res_a.diagnostics.empty() ? "" : res_a.diagnostics.front().message);

    Engine monitor_engine;
    auto res_b = monitor_engine.build(monitor, sr);
    ASSERT_TRUE(res_b.ok) << (res_b.diagnostics.empty() ? "" : res_b.diagnostics.front().message);

    // Singleton proof: both engines hold THE instance.
    EXPECT_EQ(control_engine.step_for(dev_a), device.get());
    EXPECT_EQ(monitor_engine.step_for(dev_b), device.get());

    auto* source = dynamic_cast<SourceStep*>(control_engine.step_for(src_id));
    auto* sink   = dynamic_cast<SinkStep*>(monitor_engine.step_for(snk_id));
    ASSERT_NE(source, nullptr);
    ASSERT_NE(sink,   nullptr);

    // Cycle 1: command 3.5 through the control pipeline, observe it in
    // the monitor pipeline on the next read.
    source->set(3.5);
    control_engine.play();                     // write: device.state <- 3.5
    EXPECT_DOUBLE_EQ(device->state(), 3.5);    // control wiring resolved (post-B build!)
    monitor_engine.play();                     // read: measured <- state; sink sees it
    EXPECT_DOUBLE_EQ(sink->last(), 3.5);       // monitor wiring resolved

    // Cycle 2: interleaved ticks keep resolving against the right engine.
    source->set(-1.25);
    control_engine.play();
    monitor_engine.play();
    EXPECT_DOUBLE_EQ(device->state(), -1.25);
    EXPECT_DOUBLE_EQ(sink->last(), -1.25);
}
