#include <gtest/gtest.h>

#include "piper/attribute.h"
#include "piper/graph.h"
#include "piper/node_type.h"
#include "piper/stage.h"

#include "piper/engine/builtin_steps.h"
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
    // A device-style step: publishes a measurement in "read" and consumes a
    // command in "write" (per-pin stages) -- the Bus pattern from
    // docs/v2_format.md, one physical device spanning two stages.
    class MotorStep final : public Step
    {
    public:
        static constexpr auto READ  = hash_name("read");
        static constexpr auto WRITE = hash_name("write");

        void declare_io() override
        {
            declare_output<double>("position", position_);
            declare_input<double>("torque_cmd");
        }

        void compute(piper::engine::Stage current) override
        {
            if (current.id == READ)
            {
                position_ = ++revs_;
            }
            else if (current.id == WRITE)
            {
                applied_ = input<double>("torque_cmd");
            }
        }

        double applied() const { return applied_; }

    private:
        double position_{0.0};
        double revs_{0.0};
        double applied_{0.0};
    };

    // A transform-style step: forwards the measurement in "read", emits the
    // command in "control". Its torque output feeds BACK into the motor --
    // the bidirectional cross-stage pair.
    class TransformStep final : public Step
    {
    public:
        static constexpr auto READ    = hash_name("read");
        static constexpr auto CONTROL = hash_name("control");

        void declare_io() override
        {
            declare_input<double>("position_in");
            declare_output<double>("torque_out", torque_);
        }

        void compute(piper::engine::Stage current) override
        {
            if (current.id == READ)
            {
                seen_at_read_ = input<double>("position_in");
            }
            else if (current.id == CONTROL)
            {
                torque_ = seen_at_read_ * 2.0;
            }
        }

        double seen_at_read() const { return seen_at_read_; }

    private:
        double torque_{0.0};
        double seen_at_read_{0.0};
    };

    NodeType make_motor_meta()
    {
        NodeType nt;
        nt.type       = "test_motor";
        nt.category   = "test";
        nt.attributes = {
            { "position",   "double", AttributeSpec::Role::Output, "" },
            { "torque_cmd", "double", AttributeSpec::Role::Input,  "" },
        };
        return nt;
    }

    NodeType make_transform_meta()
    {
        NodeType nt;
        nt.type       = "test_transform";
        nt.category   = "test";
        nt.attributes = {
            { "position_in", "double", AttributeSpec::Role::Input,  "" },
            { "torque_out",  "double", AttributeSpec::Role::Output, "" },
        };
        return nt;
    }
}

// motor.position[read] -> transform.position_in, and
// transform.torque_out[control] -> motor.torque_cmd[write].
// Both nodes are active in "read", but the back-link's pins are not: it must
// order nothing there. Filtering links by node membership alone drags the
// back-link into "read" and reports a false cycle.
TEST(EngineBuild, CrossStageBackLinkIsNotACycle)
{
    Graph g;
    g.add_stage(piper::Stage{ "read",    0xFFFFFFFFu });
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    g.add_stage(piper::Stage{ "write",   0xFFFFFFFFu });

    // Transform first: with the forward link filtered away too, NodeId
    // tie-breaking would run it before the motor in "read" and it would see
    // a stale position -- so this order also guards the forward edge.
    auto transform_id = g.add_node(piper_engine_test::make_transform_meta(),
                                   "transform", "read", Point{ 1.0f, 0.0f });
    auto motor_id     = g.add_node(piper_engine_test::make_motor_meta(),
                                   "motor", "read", Point{ 0.0f, 0.0f });

    ASSERT_TRUE(g.set_attr_stages(motor_id, "torque_cmd",
                                  std::vector<std::string>{ "write" }));
    ASSERT_TRUE(g.set_attr_stages(transform_id, "torque_out",
                                  std::vector<std::string>{ "control" }));

    g.add_link(PinRef{ motor_id, "position" },       PinRef{ transform_id, "position_in" }, "double");
    g.add_link(PinRef{ transform_id, "torque_out" }, PinRef{ motor_id, "torque_cmd" },      "double");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);
    sr.add("test_motor",     [] { return std::make_shared<piper_engine_test::MotorStep>(); });
    sr.add("test_transform", [] { return std::make_shared<piper_engine_test::TransformStep>(); });

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok) << (res.diagnostics.empty() ? "" : res.diagnostics.front().message);

    auto* motor     = dynamic_cast<piper_engine_test::MotorStep*>(e.step_for(motor_id));
    auto* transform = dynamic_cast<piper_engine_test::TransformStep*>(e.step_for(transform_id));
    ASSERT_NE(motor,     nullptr);
    ASSERT_NE(transform, nullptr);

    for (int i = 1; i <= 3; ++i)
    {
        e.tick("read");
        // Forward link still orders "read": the transform sees THIS tick's
        // measurement, not last tick's.
        EXPECT_EQ(transform->seen_at_read(), static_cast<double>(i));
        e.tick("control");
        e.tick("write");
        EXPECT_EQ(motor->applied(), 2.0 * i);
    }
}
