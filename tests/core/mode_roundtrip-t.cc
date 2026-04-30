#include <gtest/gtest.h>

#include "piper/registry.h"
#include "piper/serialize_v2.h"

using namespace piper;

NodeType make_dummy_type()
{
    NodeType nt;
    nt.type = "Dummy";
    return nt;
}

// core is color-table-independent: a fabricated mode label round-trips
// without any GUI-side mode_color_table being instantiated.
TEST(ModeRoundTrip, CustomLabelSurvives)
{
    NodeRegistry r;
    r.add(make_dummy_type());

    Graph g;
    auto a = g.add_node(make_dummy_type(), "a", {});
    auto b = g.add_node(make_dummy_type(), "b", {});

    ModeProfile p;
    p.name             = "engineering";
    p.per_node[a]      = "custom_safety_mode";
    p.per_node[b]      = "engine_specific_label";
    g.add_mode_profile(p);

    auto loaded = v2::deserialize(v2::serialize(g), r);
    EXPECT_TRUE(loaded.diagnostics.empty());
    ASSERT_EQ(loaded.graph.mode_profiles().size(), 1u);
    EXPECT_TRUE(loaded.graph.default_mode_name().empty());

    auto const& m = loaded.graph.mode_profiles()[0];
    EXPECT_EQ(m.name, "engineering");
    EXPECT_EQ(m.per_node.size(), 2u);
    EXPECT_EQ(m.per_node.at(a), "custom_safety_mode");
    EXPECT_EQ(m.per_node.at(b), "engine_specific_label");
}

TEST(ModeRoundTrip, BuiltInLabelsSurvive)
{
    NodeRegistry r;
    r.add(make_dummy_type());

    Graph g;
    auto a = g.add_node(make_dummy_type(), "a", {});
    auto b = g.add_node(make_dummy_type(), "b", {});

    ModeProfile p;
    p.name        = "default";
    p.per_node[a] = "enable";
    p.per_node[b] = "disable";
    g.add_mode_profile(p);
    g.set_default_mode_name("default");

    auto loaded = v2::deserialize(v2::serialize(g), r);
    EXPECT_TRUE(loaded.diagnostics.empty());
    ASSERT_EQ(loaded.graph.mode_profiles().size(), 1u);
    auto const& m = loaded.graph.mode_profiles()[0];
    EXPECT_EQ(m.per_node.at(a), "enable");
    EXPECT_EQ(m.per_node.at(b), "disable");
    EXPECT_EQ(loaded.graph.default_mode_name(), "default");
}

TEST(ModeRoundTrip, MultipleProfilesWithMixedLabels)
{
    NodeRegistry r;
    r.add(make_dummy_type());

    Graph g;
    auto a = g.add_node(make_dummy_type(), "a", {});

    ModeProfile p1;
    p1.name        = "default";
    p1.per_node[a] = "enable";

    ModeProfile p2;
    p2.name        = "v1_mode";
    p2.per_node[a] = "neutral";  // V1's third option, opaque to V2 core

    ModeProfile p3;
    p3.name        = "experimental";
    p3.per_node[a] = "passthrough_x42";  // arbitrary engine label

    g.add_mode_profile(p1);
    g.add_mode_profile(p2);
    g.add_mode_profile(p3);
    g.set_default_mode_name("default");

    auto loaded = v2::deserialize(v2::serialize(g), r);
    EXPECT_TRUE(loaded.diagnostics.empty());
    ASSERT_EQ(loaded.graph.mode_profiles().size(), 3u);
    EXPECT_EQ(loaded.graph.mode_profiles()[1].per_node.at(a), "neutral");
    EXPECT_EQ(loaded.graph.mode_profiles()[2].per_node.at(a), "passthrough_x42");
}
