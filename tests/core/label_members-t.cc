#include <gtest/gtest.h>

#include "piper/commands.h"
#include "piper/command_stack.h"
#include "piper/graph.h"
#include "piper/registry.h"
#include "piper/serialize_v2.h"

namespace
{
    struct LabelMembers : testing::Test
    {
        piper::Graph graph;
        piper::NodeRegistry registry;
        piper::NodeId id;
        void SetUp() override
        {
            piper::NodeType type;
            type.type = "filter";
            type.attributes = {
                {"in", "float", piper::AttributeSpec::Role::Input, ""},
                {"cutoff", "float", piper::AttributeSpec::Role::Member, "10"},
                {"dt", "double", piper::AttributeSpec::Role::Member, "0.001"},
            };
            registry.add(type);
            id = graph.add_node(type, "filter", "", {});
            graph.add_mode_profile({"slow", {{id, "cutoff_10Hz"}}});
            graph.add_mode_profile({"fast", {{id, "cutoff_100Hz"}}});
        }
        piper::Node const& node()
        {
            return *graph.find_node(id);
        }
    };
}

TEST_F(LabelMembers, ConversionCopiesEveryMemberAndEditsAreIndependent)
{
    ASSERT_TRUE(graph.set_members_per_label(id, true));
    ASSERT_EQ(node().label_members.size(), 5u);
    EXPECT_EQ(node().label_members.at("cutoff_100Hz"), node().shared_members());
    ASSERT_TRUE(graph.set_label_member(id, "cutoff_100Hz", "cutoff", "100"));
    EXPECT_EQ(node().members_for_label("cutoff_100Hz").at("cutoff"), "100");
    EXPECT_EQ(node().members_for_label("cutoff_10Hz").at("cutoff"), "10");
    EXPECT_EQ(node().shared_members().at("cutoff"), "10");
    ASSERT_TRUE(graph.set_label_member(id, "cutoff_100Hz", "dt", "0.002"));
    EXPECT_EQ(node().members_for_label("cutoff_10Hz").at("dt"), "0.001");
    EXPECT_THROW(node().members_for_label("unknown"), std::out_of_range);
    ASSERT_TRUE(graph.set_label_member(id, "", "cutoff", "15"));
    ASSERT_TRUE(graph.set_label_member(id, "disable", "cutoff", "25"));
    ASSERT_TRUE(graph.set_attr_value(id, "cutoff", "999"));
    EXPECT_EQ(node().members_for_label("").at("cutoff"), "15");
    EXPECT_EQ(node().members_for_label("disable").at("cutoff"), "25");
    EXPECT_EQ(node().members_for_label("enable").at("cutoff"), "10");
    EXPECT_EQ(node().members_for_label("cutoff_10Hz").at("cutoff"), "10");
}

TEST_F(LabelMembers, CollapseKeepsChosenLabelsCompleteSet)
{
    ASSERT_TRUE(graph.set_members_per_label(id, true));
    ASSERT_TRUE(graph.set_label_member(id, "cutoff_100Hz", "cutoff", "100"));
    graph.remove_mode_profile("fast");
    EXPECT_EQ(node().label_members.at("cutoff_100Hz").at("cutoff"), "100");
    EXPECT_FALSE(graph.set_members_per_label(id, false, "missing"));
    ASSERT_TRUE(graph.set_members_per_label(id, false, "cutoff_100Hz"));
    EXPECT_FALSE(node().members_per_label);
    EXPECT_TRUE(node().label_members.empty());
    EXPECT_EQ(node().shared_members().at("cutoff"), "100");
}

TEST_F(LabelMembers, RejectsPartialSetsAndNonMemberKeys)
{
    ASSERT_TRUE(graph.set_members_per_label(id, true));
    auto labels = node().label_members;
    labels.at("cutoff_100Hz").erase("dt");
    EXPECT_FALSE(graph.set_node_members(id, true, node().shared_members(), labels));
    labels = node().label_members;
    labels.at("cutoff_100Hz")["in"] = "1";
    EXPECT_FALSE(graph.set_node_members(id, true, node().shared_members(), labels));
    EXPECT_FALSE(graph.set_label_member(id, "cutoff_10Hz", "in", "1"));
}

TEST_F(LabelMembers, WholeConfigurationUndoRedoAndNodeDeletion)
{
    piper::CommandStack stack;
    auto before = node();
    ASSERT_TRUE(graph.set_members_per_label(id, true));
    ASSERT_TRUE(graph.set_label_member(id, "cutoff_100Hz", "cutoff", "100"));
    auto configured = node();
    ASSERT_TRUE(graph.set_members_per_label(id, false, "cutoff_100Hz"));
    auto collapsed = node();
    graph.find_node_mut(id)->attrs = before.attrs;
    graph.find_node_mut(id)->members_per_label = before.members_per_label;
    graph.find_node_mut(id)->label_members = before.label_members;
    stack.push(std::make_unique<piper::SetNodeMembersCommand>(id, configured), graph);
    stack.push(std::make_unique<piper::SetNodeMembersCommand>(id, collapsed), graph);
    stack.undo(graph);
    EXPECT_TRUE(node().members_per_label);
    EXPECT_EQ(node().label_members, configured.label_members);
    stack.undo(graph);
    EXPECT_FALSE(node().members_per_label);
    EXPECT_EQ(node().shared_members(), before.shared_members());
    stack.redo(graph);
    stack.push(std::make_unique<piper::DeleteNodeCommand>(id), graph);
    stack.undo(graph);
    EXPECT_EQ(node().label_members, configured.label_members);
}

TEST_F(LabelMembers, TypedRoundTripAndMalformedSetDiagnostic)
{
    ASSERT_TRUE(graph.set_members_per_label(id, true));
    ASSERT_TRUE(graph.set_label_member(id, "cutoff_100Hz", "cutoff", "100"));
    auto text = piper::v2::serialize(graph);
    EXPECT_NE(text.find("\"cutoff\": 100"), std::string::npos);
    auto loaded = piper::v2::deserialize(text, registry);
    ASSERT_TRUE(loaded.diagnostics.empty());
    EXPECT_EQ(loaded.graph.find_node(id)->label_members, node().label_members);
    graph.find_node_mut(id)->label_members.at("cutoff_100Hz").erase("dt");
    loaded = piper::v2::deserialize(piper::v2::serialize(graph), registry);
    ASSERT_NE(loaded.graph.find_node(id), nullptr);
    ASSERT_FALSE(loaded.diagnostics.empty());
    EXPECT_NE(loaded.diagnostics.front().message.find("cutoff_100Hz"), std::string::npos);
    EXPECT_EQ(loaded.graph.find_node(id)->label_members, node().label_members);
}

TEST_F(LabelMembers, LegacyFilesStaySharedAndConfiguredLabelsAreAdvertised)
{
    auto loaded = piper::v2::deserialize(piper::v2::serialize(graph), registry);
    ASSERT_TRUE(loaded.diagnostics.empty());
    EXPECT_FALSE(loaded.graph.find_node(id)->members_per_label);
    ASSERT_TRUE(graph.set_members_per_label(id, true));
    auto labels = piper::mode_labels_advertised_by(node(), registry);
    EXPECT_EQ(labels, (std::vector<std::string>{"cutoff_100Hz", "cutoff_10Hz"}));
}

TEST(LabelMembersRegistry, LabelStringMembersAreReadFromSelectedSet)
{
    piper::NodeRegistry registry;
    piper::NodeType type;
    type.type = "custom";
    type.attributes = {{"label", "string", piper::AttributeSpec::Role::Member, "base", true}};
    registry.add(type);
    piper::Graph graph;
    auto id = graph.add_node(type, "node", "", {});
    graph.set_members_per_label(id, true);
    graph.set_label_member(id, "active", "label", "local_label");
    EXPECT_EQ(piper::mode_labels_advertised_by(*graph.find_node(id), registry, "active"),
              (std::vector<std::string>{"active", "local_label"}));
}

TEST_F(LabelMembers, EveryAvailableLabelExistsImmediatelyAndRemainsIndependent)
{
    ASSERT_TRUE(graph.set_members_per_label(id, true, "", {"neutral", "custom"}));
    for (auto label : {"", "enable", "disable", "neutral", "custom", "cutoff_10Hz", "cutoff_100Hz"})
    {
        ASSERT_EQ(node().label_members.count(label), 1u);
        EXPECT_EQ(node().label_members.at(label), node().shared_members());
    }
    ASSERT_TRUE(graph.set_label_member(id, "neutral", "cutoff", "25"));
    EXPECT_EQ(node().members_for_label("enable").at("cutoff"), "10");
    EXPECT_EQ(node().members_for_label("neutral").at("cutoff"), "25");
}

TEST_F(LabelMembers, NewlyAssignedLabelsAutomaticallyReceiveEveryMember)
{
    ASSERT_TRUE(graph.set_members_per_label(id, true));
    auto const initial = node().shared_members();
    ASSERT_TRUE(graph.set_node_mode_label("slow", id, "neutral"));
    ASSERT_EQ(node().label_members.count("neutral"), 1u);
    EXPECT_EQ(node().label_members.at("neutral"), initial);
    ASSERT_TRUE(graph.set_label_member(id, "neutral", "cutoff", "25"));
    EXPECT_EQ(node().members_for_label("neutral").at("dt"), initial.at("dt"));
    EXPECT_EQ(node().members_for_label("enable"), initial);
    EXPECT_EQ(node().shared_members(), initial);
    ASSERT_TRUE(graph.add_mode_profile({"new_profile", {{id, "new_label"}}}));
    EXPECT_EQ(node().label_members.at("new_label"), initial);
}

TEST_F(LabelMembers, PerLabelModeDoesNotReadSharedValues)
{
    ASSERT_TRUE(graph.set_members_per_label(id, true, "", {"neutral"}));
    ASSERT_TRUE(graph.set_attr_value(id, "cutoff", "999"));
    for (auto const& [label, values] : node().label_members)
    {
        EXPECT_EQ(node().members_for_label(label).at("cutoff"), "10");
    }
    piper::CommandStack stack;
    auto changed = node();
    changed.label_members.at("neutral")["cutoff"] = "25";
    stack.push(std::make_unique<piper::SetNodeMembersCommand>(id, changed), graph);
    EXPECT_EQ(node().members_for_label("neutral").at("cutoff"), "25");
    stack.undo(graph);
    EXPECT_EQ(node().members_for_label("neutral").at("cutoff"), "10");
}

TEST_F(LabelMembers, IntroducingLabelAndItsMembersUndoTogether)
{
    ASSERT_TRUE(graph.set_members_per_label(id, true));
    auto const initial = node().label_members;
    piper::CommandStack stack;
    stack.push(std::make_unique<piper::SetNodeModeLabelCommand>("slow", id, "new"), graph);
    EXPECT_EQ(node().label_members.count("new"), 1u);
    stack.undo(graph);
    EXPECT_EQ(node().label_members, initial);
    EXPECT_EQ(graph.mode_profiles().front().per_node.at(id), "cutoff_10Hz");
    stack.redo(graph);
    EXPECT_EQ(node().label_members.count("new"), 1u);
    stack.undo(graph);
    stack.push(std::make_unique<piper::AddModeProfileCommand>(
                   piper::ModeProfile{"new_profile", {{id, "new"}}}),
               graph);
    EXPECT_EQ(node().label_members.count("new"), 1u);
    stack.undo(graph);
    EXPECT_EQ(node().label_members, initial);
}

TEST_F(LabelMembers, SharedConfigurationCannotContainLabelSets)
{
    piper::LabelMemberValues labels{{"neutral", node().shared_members()}};
    EXPECT_FALSE(graph.set_node_members(id, false, node().shared_members(), labels));
    EXPECT_FALSE(node().members_per_label);
    EXPECT_TRUE(node().label_members.empty());
}
