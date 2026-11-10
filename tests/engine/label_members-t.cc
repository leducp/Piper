#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <numbers>
#include <sstream>

#include "piper/builtin_nodes.h"
#include "piper/engine/builtin_steps.h"
#include "piper/engine/engine.h"
#include "piper/serialize_v2.h"

namespace
{
    using piper::engine::Stage;
    struct RecordingStep : piper::engine::Step
    {
        int declarations{0};
        int callbacks{0};
        int ticks{0};
        float cached{0};
        float out{0};
        std::string declared_label;
        std::vector<std::string> labels;
        void declare_io() override
        {
            ++declarations;
            declared_label = current_label().name;
            EXPECT_EQ(member("value"), "10");
            declare_output<float>("out", out);
        }
        void on_label_changed() override
        {
            ++callbacks;
            cached = std::stof(member("value"));
            labels.emplace_back(current_label().name);
        }
        void compute(Stage) override
        {
            ++ticks;
            out = cached;
        }
    };

    struct EngineLabelMembers : testing::Test
    {
        piper::Graph graph;
        piper::NodeRegistry nodes;
        piper::engine::StepRegistry steps;
        piper::NodeId id;
        void SetUp() override
        {
            piper::register_builtin_nodes(nodes);
            piper::engine::register_builtin_steps(steps);
            graph.add_stage({"control", 0xFFFFFFFFu});
            piper::NodeType type;
            type.type = "recording";
            type.attributes = {{"value", "float", piper::AttributeSpec::Role::Member, "5"},
                               {"out", "float", piper::AttributeSpec::Role::Output, ""}};
            id = graph.add_node(type, "record", "control", {});
            graph.add_mode_profile({"slow", {{id, "ten"}}});
            graph.add_mode_profile({"fast", {{id, "hundred"}}});
            graph.add_mode_profile({"same", {{id, "hundred"}}});
            graph.add_mode_profile({"off", {{id, "disable"}}});
            graph.set_default_mode_name("slow");
            ASSERT_TRUE(graph.set_members_per_label(id, true));
            ASSERT_TRUE(graph.set_label_member(id, "ten", "value", "10"));
            ASSERT_TRUE(graph.set_label_member(id, "hundred", "value", "100"));
            steps.add("recording", [] { return std::make_shared<RecordingStep>(); });
        }
    };
}

TEST_F(EngineLabelMembers, InitialMembersThenCallbacksOnlyOnLabelChanges)
{
    graph.set_label_member(id, "disable", "value", "7");
    graph.set_label_member(id, "", "value", "9");
    graph.set_attr_value(id, "value", "999");
    piper::engine::Engine engine;
    ASSERT_TRUE(engine.build(graph, steps).ok);
    auto* step = dynamic_cast<RecordingStep*>(engine.step_for(id));
    ASSERT_NE(step, nullptr);
    EXPECT_EQ(step->declared_label, "ten");
    EXPECT_EQ(step->callbacks, 1);
    engine.play();
    EXPECT_EQ(step->out, 10);
    engine.set_mode("fast");
    EXPECT_EQ(step->cached, 100);
    EXPECT_EQ(step->callbacks, 2);
    engine.set_mode("same");
    engine.play();
    EXPECT_EQ(step->callbacks, 2);
    EXPECT_EQ(step->ticks, 2);
    engine.set_mode("off");
    EXPECT_EQ(step->labels.back(), "disable");
    EXPECT_EQ(step->cached, 7);
    engine.play();
    EXPECT_EQ(step->out, 100);
    EXPECT_EQ(step->ticks, 2);
    engine.set_mode("unknown");
    EXPECT_EQ(step->labels.back(), "");
    EXPECT_EQ(step->cached, 9);
    engine.play();
    EXPECT_EQ(step->ticks, 3);
    EXPECT_EQ(step->declarations, 1);
}

TEST_F(EngineLabelMembers, SharedMembersStillReceiveLabelNotifications)
{
    ASSERT_TRUE(graph.set_members_per_label(id, false, "ten"));
    piper::engine::Engine engine;
    ASSERT_TRUE(engine.build(graph, steps).ok);
    auto* step = dynamic_cast<RecordingStep*>(engine.step_for(id));
    engine.set_mode("fast");
    EXPECT_EQ(step->cached, 10);
    EXPECT_EQ(step->callbacks, 2);
}

TEST_F(EngineLabelMembers, IncompleteInactiveSetFailsBuild)
{
    graph.find_node_mut(id)->label_members.at("hundred").erase("value");
    piper::engine::Engine engine;
    auto result = engine.build(graph, steps);
    EXPECT_FALSE(result.ok);
    bool found = false;
    for (auto const& d : result.diagnostics)
    {
        if (d.kind == piper::engine::BuildDiagnostic::Kind::InvalidMemberConfiguration)
        {
            EXPECT_EQ(d.node_id, id);
            EXPECT_NE(d.message.find("hundred"), std::string::npos);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(EngineLabelMembers, MalformedNumericLabelStopsExecutionWithContext)
{
    graph.set_label_member(id, "hundred", "value", "bad");
    piper::engine::Engine engine;
    ASSERT_TRUE(engine.build(graph, steps).ok);
    auto* step = dynamic_cast<RecordingStep*>(engine.step_for(id));
    try
    {
        engine.set_mode("fast");
        FAIL() << "expected callback error";
    }
    catch (std::runtime_error const& error)
    {
        EXPECT_NE(std::string(error.what()).find("hundred"), std::string::npos);
    }
    engine.play();
    EXPECT_EQ(step->ticks, 0);
}

TEST_F(EngineLabelMembers, SharedStepKeepsLastExplicitLabelConfiguration)
{
    auto shared = std::make_shared<RecordingStep>();
    piper::engine::StepRegistry registry;
    registry.add("recording", [shared] { return shared; });
    piper::engine::Engine first, second;
    ASSERT_TRUE(first.build(graph, registry).ok);
    ASSERT_TRUE(second.build(graph, registry).ok);
    second.set_mode("fast");
    int callbacks = shared->callbacks;
    second.play();
    EXPECT_EQ(shared->out, 100);
    first.play();
    EXPECT_EQ(shared->out, 100);
    EXPECT_EQ(shared->callbacks, callbacks);

    first.set_mode("fast");
    first.set_mode("slow");
    callbacks = shared->callbacks;
    second.play();
    EXPECT_EQ(shared->out, 10);
    EXPECT_EQ(shared->callbacks, callbacks);
    // Keeping the same label does not reconfigure the shared instance.
    second.set_mode("same");
    second.play();
    EXPECT_EQ(shared->out, 10);
    EXPECT_EQ(shared->callbacks, callbacks);
}

TEST(EngineLabelFilter, ChangesCutoffWithoutResettingHistory)
{
    piper::NodeRegistry nodes;
    piper::register_builtin_nodes(nodes);
    piper::engine::StepRegistry steps;
    piper::engine::register_builtin_steps(steps);
    piper::Graph graph;
    graph.add_stage({"control", 0xFFFFFFFFu});
    auto source = graph.add_node(*nodes.find("constant<float>"), "source", "control", {});
    graph.set_attr_value(source, "value", "1");
    auto filter = graph.add_node(*nodes.find("low_pass<float>"), "filter", "control", {});
    graph.add_link({source, "out"}, {filter, "in"}, "float");
    graph.add_mode_profile({"slow", {{filter, "cutoff_10Hz"}}});
    graph.add_mode_profile({"fast", {{filter, "cutoff_100Hz"}}});
    graph.set_default_mode_name("slow");
    ASSERT_TRUE(graph.set_members_per_label(filter, true));
    graph.set_label_member(filter, "cutoff_10Hz", "cutoff", "10");
    graph.set_label_member(filter, "cutoff_100Hz", "cutoff", "100");
    piper::engine::Engine engine;
    ASSERT_TRUE(engine.build(graph, steps).ok);
    auto* step = engine.step_for(filter);
    auto alpha = [](double cutoff)
    { return 0.001 / (1.0 / (2.0 * std::numbers::pi * cutoff) + 0.001); };
    engine.play();
    double previous = step->output<float>("out");
    EXPECT_NEAR(previous, alpha(10), 1e-6);
    engine.set_mode("fast");
    EXPECT_FLOAT_EQ(step->output<float>("out"), float(previous));
    engine.play();
    EXPECT_NEAR(step->output<float>("out"), previous + alpha(100) * (1 - previous), 1e-6);
}

TEST(EngineLabelFilter, BundledExampleLoadsAndRunsBothLabels)
{
    std::ifstream file(std::string(PIPER_SOURCE_DIR) + "/examples/filter_demo/filter_demo.piper");
    ASSERT_TRUE(file.good());
    std::ostringstream text;
    text << file.rdbuf();
    piper::NodeRegistry nodes;
    piper::register_builtin_nodes(nodes);
    auto loaded = piper::v2::deserialize(text.str(), nodes);
    ASSERT_TRUE(loaded.diagnostics.empty());
    piper::engine::StepRegistry steps;
    piper::engine::register_builtin_steps(steps);
    piper::engine::Engine engine;
    ASSERT_TRUE(engine.build(loaded.graph, steps).ok);
    engine.play();
    EXPECT_EQ(engine.step_for(2)->member("cutoff"), "10");
    engine.set_mode("wide_band");
    engine.play();
    EXPECT_EQ(engine.step_for(2)->member("cutoff"), "100");
    EXPECT_TRUE(std::isfinite(engine.step_for(2)->output<float>("out")));
}

TEST(EngineLabelMembersStandalone, ExternalPointersStayFixedAcrossLabelChanges)
{
    piper::NodeRegistry nodes;
    piper::register_builtin_nodes(nodes);
    piper::engine::StepRegistry steps;
    piper::engine::register_builtin_steps(steps);
    piper::Graph graph;
    graph.add_stage({"control", 0xFFFFFFFFu});
    auto source = graph.add_node(*nodes.find("external_input<float>"), "source", "control", {});
    auto sink = graph.add_node(*nodes.find("external_output<float>"), "sink", "control", {});
    graph.set_attr_value(source, "value", "target");
    graph.set_attr_value(sink, "value", "measured");
    graph.add_link({source, "out"}, {sink, "in"}, "float");
    graph.add_mode_profile({"first", {{source, "a"}, {sink, "a"}}});
    graph.add_mode_profile({"second", {{source, "b"}, {sink, "b"}}});
    graph.add_mode_profile({"off", {{source, "disable"}, {sink, "disable"}}});
    graph.set_default_mode_name("first");
    ASSERT_TRUE(graph.set_members_per_label(source, true));
    ASSERT_TRUE(graph.set_members_per_label(sink, true));
    ASSERT_TRUE(graph.set_label_member(source, "a", "max", "10"));
    ASSERT_TRUE(graph.set_label_member(source, "b", "max", "100"));
    // External names are not members, so they cannot vary per label.
    EXPECT_FALSE(graph.set_label_member(source, "b", "value", "measured"));
    piper::engine::Engine engine;
    ASSERT_TRUE(engine.build(graph, steps).ok);
    auto* input = engine.input<float>("target");
    auto const* output = engine.output<float>("measured");
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);
    *input = 42;
    engine.play();
    EXPECT_EQ(*output, 42);
    engine.set_mode("second");
    EXPECT_EQ(engine.step_for(source)->member("max"), "100");
    EXPECT_EQ(engine.input<float>("target"), input);
    EXPECT_EQ(engine.output<float>("measured"), output);
    EXPECT_EQ(engine.input<float>("measured"), nullptr);
    *input = 75;
    engine.play();
    EXPECT_EQ(*output, 75);
    engine.set_mode("off");
    EXPECT_EQ(engine.input<float>("target"), input);
    EXPECT_EQ(engine.output<float>("measured"), output);
    *input = 99;
    engine.play();
    EXPECT_EQ(*output, 75);
    engine.set_mode("first");
    engine.play();
    EXPECT_EQ(*output, 99);
    EXPECT_EQ(engine.step_for(source)->member("max"), "10");
}

TEST(EngineLabelMembersStandalone, ConstantHoldsOutputWhileDisabled)
{
    piper::NodeRegistry nodes;
    piper::register_builtin_nodes(nodes);
    piper::engine::StepRegistry steps;
    piper::engine::register_builtin_steps(steps);
    piper::Graph graph;
    graph.add_stage({"control", 0xFFFFFFFFu});
    auto id = graph.add_node(*nodes.find("constant<float>"), "source", "control", {});
    graph.set_attr_value(id, "value", "5");
    graph.add_mode_profile({"on", {{id, "hundred"}}});
    graph.add_mode_profile({"off", {{id, "disable"}}});
    graph.set_default_mode_name("on");
    graph.set_members_per_label(id, true);
    graph.set_label_member(id, "hundred", "value", "100");
    piper::engine::Engine engine;
    ASSERT_TRUE(engine.build(graph, steps).ok);
    auto* step = engine.step_for(id);
    EXPECT_FLOAT_EQ(step->output<float>("out"), 100);
    engine.set_mode("off");
    EXPECT_EQ(step->member("value"), "5");
    engine.play();
    EXPECT_FLOAT_EQ(step->output<float>("out"), 100);
    engine.set_mode("unknown");
    EXPECT_FLOAT_EQ(step->output<float>("out"), 5);
}

TEST_F(EngineLabelMembers, MissingAssignedSetFailsInsteadOfReadingSharedValues)
{
    graph.find_node_mut(id)->label_members.erase("hundred");
    piper::engine::Engine engine;
    auto result = engine.build(graph, steps);
    EXPECT_FALSE(result.ok);
    bool found = false;
    for (auto const& diagnostic : result.diagnostics)
    {
        if (diagnostic.kind == piper::engine::BuildDiagnostic::Kind::InvalidMemberConfiguration)
        {
            EXPECT_NE(diagnostic.message.find("hundred"), std::string::npos);
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(EngineLabelMembers, MemberSnapshotIsIndependentOfSourceGraph)
{
    piper::engine::Engine engine;
    ASSERT_TRUE(engine.build(graph, steps).ok);
    graph.set_label_member(id, "hundred", "value", "200");
    engine.set_mode("fast");
    engine.play();
    EXPECT_FLOAT_EQ(engine.step_for(id)->output<float>("out"), 100);

    ASSERT_TRUE(engine.build(graph, steps).ok);
    engine.set_mode("fast");
    engine.play();
    EXPECT_FLOAT_EQ(engine.step_for(id)->output<float>("out"), 200);
}
