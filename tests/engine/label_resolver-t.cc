#include <gtest/gtest.h>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/label.h"
#include "piper/registry.h"

#include "piper/engine/label_resolver.h"

using piper::Graph;
using piper::Label;
using piper::LabelKind;
using piper::NodeRegistry;
using piper::PinRef;
using piper::Point;
using piper::engine::resolve_label_clusters;
using piper::engine::BuildDiagnostic;

namespace
{
    Graph make_graph_with_constant(NodeRegistry const& nr)
    {
        Graph g;
        g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
        auto const* cf = nr.find("constant<float>");
        EXPECT_NE(cf, nullptr);
        g.add_node(*cf, "src", "control", Point{ 0.0f, 0.0f });
        return g;
    }
}

TEST(LabelResolver, EmptyGraphProducesEmptyEffectiveLinks)
{
    Graph g;
    std::vector<BuildDiagnostic> diags;
    bool has_error = false;
    auto effective = resolve_label_clusters(g, diags, has_error);
    EXPECT_TRUE(effective.empty());
    EXPECT_TRUE(diags.empty());
    EXPECT_FALSE(has_error);
}

TEST(LabelResolver, RealLinksPassThrough)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);
    auto g       = make_graph_with_constant(nr);
    auto src_id  = g.nodes().front().id;
    auto const* probe = nr.find("external_output<float>");
    auto probe_id = g.add_node(*probe, "probe", "control", Point{ 1.0f, 0.0f });
    auto link_id  = g.add_link(PinRef{ src_id, "out" }, PinRef{ probe_id, "in" }, "float");

    std::vector<BuildDiagnostic> diags;
    bool has_error = false;
    auto effective = resolve_label_clusters(g, diags, has_error);

    ASSERT_EQ(effective.size(), 1u);
    EXPECT_EQ(effective.front().id, link_id);
    EXPECT_FALSE(has_error);
}

TEST(LabelResolver, SyntheticLinkBypassesLabels)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);
    auto g            = make_graph_with_constant(nr);
    auto src_id       = g.nodes().front().id;
    auto const* probe = nr.find("external_output<float>");
    auto probe_id     = g.add_node(*probe, "probe", "control", Point{ 4.0f, 0.0f });
    auto in_id        = g.add_label(LabelKind::In,  "tap", Point{ 1.0f, 0.0f });
    auto out_id       = g.add_label(LabelKind::Out, "tap", Point{ 3.0f, 0.0f });

    g.add_link(PinRef{ src_id, "out" },                  PinRef{ in_id,    piper::label_pin_name }, "float");
    g.add_link(PinRef{ out_id, piper::label_pin_name },  PinRef{ probe_id, "in" },                  "float");

    std::vector<BuildDiagnostic> diags;
    bool has_error = false;
    auto effective = resolve_label_clusters(g, diags, has_error);

    // Both label-touching links are dropped; one synthetic link replaces
    // them, going src.out -> probe.in directly.
    ASSERT_EQ(effective.size(), 1u);
    EXPECT_EQ(effective.front().from.node, src_id);
    EXPECT_EQ(effective.front().to.node,   probe_id);
    EXPECT_EQ(effective.front().id, piper::invalid_link_id);
    EXPECT_FALSE(has_error);
}

TEST(LabelResolver, NoLabelInForLabelOutFlagsError)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);
    auto g            = make_graph_with_constant(nr);
    auto const* probe = nr.find("external_output<float>");
    auto probe_id     = g.add_node(*probe, "probe", "control", Point{ 2.0f, 0.0f });
    auto out_id       = g.add_label(LabelKind::Out, "orphan", Point{ 1.0f, 0.0f });
    g.add_link(PinRef{ out_id, piper::label_pin_name }, PinRef{ probe_id, "in" }, "float");

    std::vector<BuildDiagnostic> diags;
    bool has_error = false;
    auto effective = resolve_label_clusters(g, diags, has_error);

    EXPECT_TRUE(has_error);
    EXPECT_TRUE(effective.empty());
}

TEST(LabelResolver, ChainedLabelClustersFlagError)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);
    auto g            = make_graph_with_constant(nr);
    auto src_id       = g.nodes().front().id;
    auto const* probe = nr.find("external_output<float>");
    auto probe_id     = g.add_node(*probe, "probe", "control", Point{ 5.0f, 0.0f });
    auto in_b         = g.add_label(LabelKind::In,  "b", Point{ 1.0f, 0.0f });
    auto out_b        = g.add_label(LabelKind::Out, "b", Point{ 2.0f, 0.0f });
    auto in_a         = g.add_label(LabelKind::In,  "a", Point{ 3.0f, 0.0f });
    auto out_a        = g.add_label(LabelKind::Out, "a", Point{ 4.0f, 0.0f });

    // Cluster "a" is fed by cluster "b"'s label_out: label chaining.
    g.add_link(PinRef{ src_id, "out" },                 PinRef{ in_b,     piper::label_pin_name }, "float");
    g.add_link(PinRef{ out_b, piper::label_pin_name },  PinRef{ in_a,     piper::label_pin_name }, "float");
    g.add_link(PinRef{ out_a, piper::label_pin_name },  PinRef{ probe_id, "in" },                  "float");

    std::vector<BuildDiagnostic> diags;
    bool has_error = false;
    resolve_label_clusters(g, diags, has_error);

    EXPECT_TRUE(has_error);
    bool fed_by_label = false;
    for (auto const& d : diags)
    {
        if (d.message.find("fed by another label") != std::string::npos)
        {
            fed_by_label = true;
            EXPECT_EQ(d.node_id, in_a);
        }
    }
    EXPECT_TRUE(fed_by_label);
}

TEST(LabelResolver, LabelInWithNoWiredSourceFlagsError)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);
    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto const* probe = nr.find("external_output<float>");
    auto probe_id     = g.add_node(*probe, "probe", "control", Point{ 2.0f, 0.0f });
    auto in_id        = g.add_label(LabelKind::In,  "tap", Point{ 0.0f, 0.0f });
    auto out_id       = g.add_label(LabelKind::Out, "tap", Point{ 1.0f, 0.0f });
    g.add_link(PinRef{ out_id, piper::label_pin_name }, PinRef{ probe_id, "in" }, "float");

    std::vector<BuildDiagnostic> diags;
    bool has_error = false;
    auto effective = resolve_label_clusters(g, diags, has_error);

    EXPECT_TRUE(has_error);
    EXPECT_TRUE(effective.empty());
    bool no_source = false;
    for (auto const& d : diags)
    {
        if (d.message.find("no wired source") != std::string::npos)
        {
            no_source = true;
            EXPECT_EQ(d.node_id, in_id);
        }
    }
    EXPECT_TRUE(no_source);
}

TEST(LabelResolver, LabelInWithoutLabelOutFlagsError)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);
    auto g      = make_graph_with_constant(nr);
    auto src_id = g.nodes().front().id;
    auto in_id  = g.add_label(LabelKind::In, "solo", Point{ 1.0f, 0.0f });
    g.add_link(PinRef{ src_id, "out" }, PinRef{ in_id, piper::label_pin_name }, "float");

    std::vector<BuildDiagnostic> diags;
    bool has_error = false;
    auto effective = resolve_label_clusters(g, diags, has_error);

    EXPECT_TRUE(has_error);
    EXPECT_TRUE(effective.empty());
    bool no_out = false;
    for (auto const& d : diags)
    {
        if (d.message.find("no label_out") != std::string::npos)
        {
            no_out = true;
            EXPECT_EQ(d.node_id, in_id);
            EXPECT_EQ(d.kind, BuildDiagnostic::Kind::UnresolvedInput);
        }
    }
    EXPECT_TRUE(no_out);
}

TEST(LabelResolver, SynthesizesOneLinkPerSink)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);
    auto g            = make_graph_with_constant(nr);
    auto src_id       = g.nodes().front().id;
    auto const* probe = nr.find("external_output<float>");
    auto p1           = g.add_node(*probe, "p1", "control", Point{ 4.0f, 0.0f });
    auto p2           = g.add_node(*probe, "p2", "control", Point{ 4.0f, 1.0f });
    auto in_id        = g.add_label(LabelKind::In,  "fan", Point{ 1.0f, 0.0f });
    auto a            = g.add_label(LabelKind::Out, "fan", Point{ 3.0f, 0.0f });
    auto b            = g.add_label(LabelKind::Out, "fan", Point{ 3.0f, 1.0f });

    g.add_link(PinRef{ src_id, "out" }, PinRef{ in_id, piper::label_pin_name }, "float");
    g.add_link(PinRef{ a, piper::label_pin_name }, PinRef{ p1, "in" }, "float");
    g.add_link(PinRef{ b, piper::label_pin_name }, PinRef{ p2, "in" }, "float");

    std::vector<BuildDiagnostic> diags;
    bool has_error = false;
    auto effective = resolve_label_clusters(g, diags, has_error);

    EXPECT_FALSE(has_error);
    ASSERT_EQ(effective.size(), 2u);
    for (auto const& l : effective)
    {
        EXPECT_EQ(l.from.node, src_id);
    }
}
