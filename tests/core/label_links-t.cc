#include <gtest/gtest.h>

#include "piper/label.h"
#include "piper/registry.h"
#include "piper/serialize_v2.h"

#include "test_helpers.h"

using namespace piper;
using piper::fixtures::any_of_kind;
using piper::fixtures::make_simple_type;

// A node wired to a first-class Label (link endpoint attr "pin") must
// survive serialize -> deserialize with the link intact and zero
// diagnostics. Labels now parse before links; previously every such
// link was dropped as LinkOrphanedNode.
TEST(LabelLinks, RoundTripPreservesLabelLinks)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    Graph g;
    auto a_id   = g.add_node(simple, "a", "", Point{ 0.0f, 0.0f });
    auto b_id   = g.add_node(simple, "b", "", Point{ 3.0f, 0.0f });
    auto in_id  = g.add_label(LabelKind::In,  "tap", Point{ 1.0f, 0.0f });
    auto out_id = g.add_label(LabelKind::Out, "tap", Point{ 2.0f, 0.0f });

    auto l1 = g.add_link({ a_id, "out" }, { in_id, label_pin_name }, "float");
    auto l2 = g.add_link({ out_id, label_pin_name }, { b_id, "in" }, "float");
    ASSERT_NE(l1, invalid_link_id);
    ASSERT_NE(l2, invalid_link_id);

    auto loaded = v2::deserialize(v2::serialize(g), r);

    EXPECT_TRUE(loaded.diagnostics.empty())
        << "unexpected diagnostics on label-link round-trip";
    ASSERT_EQ(loaded.graph.labels().size(), 2u);
    ASSERT_EQ(loaded.graph.links().size(),  2u);

    Link const* la = loaded.graph.find_link(l1);
    Link const* lb = loaded.graph.find_link(l2);
    ASSERT_NE(la, nullptr);
    ASSERT_NE(lb, nullptr);
    EXPECT_EQ(la->from.node, a_id);
    EXPECT_EQ(la->from.attr, "out");
    EXPECT_EQ(la->to.node,   in_id);
    EXPECT_EQ(la->to.attr,   label_pin_name);
    EXPECT_EQ(la->data_type, "float");
    EXPECT_EQ(lb->from.node, out_id);
    EXPECT_EQ(lb->from.attr, label_pin_name);
    EXPECT_EQ(lb->to.node,   b_id);
    EXPECT_EQ(lb->to.attr,   "in");
    EXPECT_EQ(lb->data_type, "float");
}

// Two same-name labels with out-of-order ids and different colors:
// after load both must carry the SMALLEST-id label's color, with a
// LabelClusterRepaired diagnostic.
TEST(LabelLinks, RepairLabelClustersAdoptsSmallestIdColor)
{
    NodeRegistry r;

    std::string const text = R"({
        "version": 3,
        "pipelines": [{
            "nodes": [],
            "links": [],
            "stages": [],
            "modes": [],
            "labels": [
                { "id": 9, "kind": "in",  "name": "tap", "pos": [0, 0],
                  "color": "#AABBCCDD" },
                { "id": 3, "kind": "out", "name": "tap", "pos": [1, 0],
                  "color": "#11223344" }
            ]
        }]
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics,
                            Diagnostic::Kind::LabelClusterRepaired));

    Label const* high = loaded.graph.find_label(9);
    Label const* low  = loaded.graph.find_label(3);
    ASSERT_NE(high, nullptr);
    ASSERT_NE(low,  nullptr);
    EXPECT_EQ(low->color.value,  0x11223344u);
    EXPECT_EQ(high->color.value, 0x11223344u) << "cluster must adopt smallest-id color";
}

TEST(LabelLinks, ConsistentClusterLoadsWithoutRepairDiagnostic)
{
    NodeRegistry r;
    Graph g;
    auto a_id = g.add_label(LabelKind::In,  "tap", Point{});
    auto b_id = g.add_label(LabelKind::Out, "tap", Point{});
    rgba const c = rgba::from_components(0x12, 0x34, 0x56, 0x78);
    g.set_label_color(a_id, c);
    g.set_label_color(b_id, c);

    auto loaded = v2::deserialize(v2::serialize(g), r);
    EXPECT_FALSE(any_of_kind(loaded.diagnostics,
                             Diagnostic::Kind::LabelClusterRepaired));
    EXPECT_TRUE(loaded.diagnostics.empty());
}
