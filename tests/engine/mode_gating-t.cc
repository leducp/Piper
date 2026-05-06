#include <gtest/gtest.h>

#include <numbers>
#include <string>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/mode_profile.h"
#include "piper/registry.h"
#include "piper/stage.h"

#include "piper/engine/builtin_steps.h"
#include "piper/engine/engine.h"
#include "piper/engine/external_io.h"
#include "piper/engine/mode.h"
#include "piper/engine/registry.h"
#include "piper/engine/stage.h"

using piper::Graph;
using piper::ModeProfile;
using piper::NodeRegistry;
using piper::PinRef;
using piper::Point;
using piper::engine::Engine;
using piper::engine::Mode;
using piper::engine::step::Output;
using piper::engine::StepRegistry;

namespace
{
    struct TwoSinesGraph
    {
        Graph         g;
        piper::NodeId a_id;
        piper::NodeId b_id;
        piper::NodeId pa_id;
        piper::NodeId pb_id;
    };

    // Two identical sin_wave nodes (phase=pi/2, so the very first
    // compute() writes 1.0) feeding two probes. Caller attaches
    // whatever modes they want before passing to the engine.
    TwoSinesGraph make_two_sines(NodeRegistry const& nr)
    {
        TwoSinesGraph t;
        t.g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

        auto const* sw = nr.find("sin_wave<float>");
        auto const* po = nr.find("external_output<float>");

        t.a_id  = t.g.add_node(*sw, "a",  "control", Point{ 0.0f, 0.0f });
        t.b_id  = t.g.add_node(*sw, "b",  "control", Point{ 0.0f, 1.0f });
        t.pa_id = t.g.add_node(*po, "pa", "control", Point{ 1.0f, 0.0f });
        t.pb_id = t.g.add_node(*po, "pb", "control", Point{ 1.0f, 1.0f });

        std::string const half_pi =
            std::to_string(std::numbers::pi_v<double> / 2.0);
        t.g.set_attr_value(t.a_id, "phase", half_pi);
        t.g.set_attr_value(t.b_id, "phase", half_pi);

        t.g.add_link(PinRef{ t.a_id, "out" }, PinRef{ t.pa_id, "in" }, "float");
        t.g.add_link(PinRef{ t.b_id, "out" }, PinRef{ t.pb_id, "in" }, "float");
        return t;
    }
}

TEST(EngineMode, DefaultModeAppliesOnBuildAndDisablesNamedNodes)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);
    auto t = make_two_sines(nr);

    ModeProfile mp;
    mp.name             = "default";
    mp.per_node[t.b_id] = "disable";
    t.g.add_mode_profile(mp);
    t.g.set_default_mode_name("default");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    ASSERT_TRUE(e.build(t.g, sr).ok);
    EXPECT_EQ(e.current_mode(), Mode{"default"});

    e.tick("control");
    auto* pa = dynamic_cast<Output<float>*>(e.step_for(t.pa_id));
    auto* pb = dynamic_cast<Output<float>*>(e.step_for(t.pb_id));
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pb, nullptr);
    EXPECT_NEAR(pa->get(), 1.0f, 1e-5f);
    EXPECT_NEAR(pb->get(), 0.0f, 1e-5f);
}

TEST(EngineMode, SetModeUngatesPreviouslyDisabledNode)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);
    auto t = make_two_sines(nr);

    ModeProfile silent;
    silent.name             = "silent";
    silent.per_node[t.b_id] = "disable";
    ModeProfile loud;
    loud.name = "loud";  // empty per_node -> nothing disabled
    t.g.add_mode_profile(silent);
    t.g.add_mode_profile(loud);
    t.g.set_default_mode_name("silent");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    ASSERT_TRUE(e.build(t.g, sr).ok);

    e.tick("control");
    auto* pb = dynamic_cast<Output<float>*>(e.step_for(t.pb_id));
    ASSERT_NE(pb, nullptr);
    EXPECT_NEAR(pb->get(), 0.0f, 1e-5f);

    e.set_mode("loud");
    EXPECT_EQ(e.current_mode(), Mode{"loud"});
    e.tick("control");
    EXPECT_NEAR(pb->get(), 1.0f, 1e-5f);
}

TEST(EngineMode, UnknownModeNameStillReportedButGatesNothing)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);
    auto t = make_two_sines(nr);

    ModeProfile mp;
    mp.name             = "default";
    mp.per_node[t.b_id] = "disable";
    t.g.add_mode_profile(mp);
    t.g.set_default_mode_name("default");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    ASSERT_TRUE(e.build(t.g, sr).ok);

    e.set_mode("undeclared_mode");
    EXPECT_EQ(e.current_mode(), Mode{"undeclared_mode"});
    e.tick("control");
    auto* pb = dynamic_cast<Output<float>*>(e.step_for(t.pb_id));
    ASSERT_NE(pb, nullptr);
    EXPECT_NEAR(pb->get(), 1.0f, 1e-5f);
}

TEST(EngineMode, NoDefaultModeMeansNoGating)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);
    auto t = make_two_sines(nr);
    // No mode_profile declared at all.

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    ASSERT_TRUE(e.build(t.g, sr).ok);
    EXPECT_TRUE(e.current_mode().empty());

    e.tick("control");
    auto* pa = dynamic_cast<Output<float>*>(e.step_for(t.pa_id));
    auto* pb = dynamic_cast<Output<float>*>(e.step_for(t.pb_id));
    ASSERT_NE(pa, nullptr);
    ASSERT_NE(pb, nullptr);
    EXPECT_NEAR(pa->get(), 1.0f, 1e-5f);
    EXPECT_NEAR(pb->get(), 1.0f, 1e-5f);
}

TEST(EngineMode, ModeAndLabelHandlesCarryHashOfTheirName)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);
    auto t = make_two_sines(nr);

    ModeProfile mp;
    mp.name             = "loose";
    mp.per_node[t.a_id] = "passthrough";
    t.g.add_mode_profile(mp);
    t.g.set_default_mode_name("loose");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    ASSERT_TRUE(e.build(t.g, sr).ok);

    auto* a = e.step_for(t.a_id);
    auto* b = e.step_for(t.b_id);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Hash matches what the user-facing API would compute for the
    // same literal -- the hot-path comparison `current_label() ==
    // "passthrough"` reduces to a uint64 compare with no string work.
    EXPECT_EQ(a->current_mode().id,  piper::engine::hash_name("loose"));
    EXPECT_EQ(a->current_label().id, piper::engine::hash_name("passthrough"));
    EXPECT_TRUE(b->current_label().empty());

    e.set_mode("");
    EXPECT_TRUE(a->current_mode().empty());
    EXPECT_EQ(a->current_mode().id, piper::engine::hash_name(""));
    EXPECT_TRUE(a->current_label().empty());
}

TEST(EngineMode, PerNodeLabelIsReadableByStepAndChangesAcrossModes)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);
    auto t = make_two_sines(nr);

    ModeProfile slow;
    slow.name             = "slow";
    slow.per_node[t.a_id] = "low_gain";
    slow.per_node[t.b_id] = "passthrough";
    ModeProfile fast;
    fast.name             = "fast";
    fast.per_node[t.a_id] = "high_gain";  // b deliberately unlabeled
    t.g.add_mode_profile(slow);
    t.g.add_mode_profile(fast);
    t.g.set_default_mode_name("slow");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    ASSERT_TRUE(e.build(t.g, sr).ok);

    auto* a = e.step_for(t.a_id);
    auto* b = e.step_for(t.b_id);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a->current_label(), Mode{"low_gain"});
    EXPECT_EQ(b->current_label(), Mode{"passthrough"});
    EXPECT_EQ(a->current_mode(),  Mode{"slow"});

    e.set_mode("fast");
    EXPECT_EQ(a->current_label(), Mode{"high_gain"});
    EXPECT_TRUE(b->current_label().empty());
    EXPECT_EQ(a->current_mode(),  Mode{"fast"});

    e.set_mode("undeclared");
    EXPECT_TRUE(a->current_label().empty());
    EXPECT_TRUE(b->current_label().empty());
}
