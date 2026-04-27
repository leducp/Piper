#include <gtest/gtest.h>

#include "piper/graph.h"
#include "piper/registry.h"

using namespace piper;

namespace
{
    NodeType make_adder()
    {
        NodeType nt;
        nt.type     = "Add";
        nt.help     = "a + b";
        nt.library  = "math";
        nt.category = "arithmetic";
        nt.attributes = {
            { "a",   "float", AttributeSpec::Role::Input,  ""    },
            { "b",   "float", AttributeSpec::Role::Input,  ""    },
            { "out", "float", AttributeSpec::Role::Output, ""    },
            { "k",   "float", AttributeSpec::Role::Member, "1.0" },
        };
        return nt;
    }
}

TEST(NodeRegistry, AddFind)
{
    NodeRegistry r;
    EXPECT_TRUE(r.empty());

    EXPECT_TRUE(r.add(make_adder()));
    EXPECT_EQ(r.size(), 1u);

    EXPECT_FALSE(r.add(make_adder()));
    EXPECT_EQ(r.size(), 1u);

    auto const* nt = r.find("Add");
    ASSERT_NE(nt, nullptr);
    EXPECT_EQ(nt->type, "Add");
    EXPECT_EQ(nt->attributes.size(), 4u);

    EXPECT_EQ(r.find("Sub"), nullptr);
}

TEST(Graph, AddNode)
{
    Graph g;
    auto adder = make_adder();

    Point const pos{ 10.0f, 20.0f };
    auto id = g.add_node(adder, "n1", "control", pos);
    EXPECT_NE(id, invalid_node_id);
    ASSERT_EQ(g.nodes().size(), 1u);

    auto const* n = g.find_node(id);
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(n->name, "n1");
    EXPECT_EQ(n->stage, "control");
    EXPECT_EQ(n->pos, pos);
    ASSERT_EQ(n->attrs.size(), 4u);
    EXPECT_EQ(n->attrs[0].name, "a");
    EXPECT_EQ(n->attrs[3].name, "k");
    EXPECT_EQ(n->attrs[3].value, "1.0");
    EXPECT_EQ(n->attrs[0].role, AttributeSpec::Role::Input);
    EXPECT_EQ(n->attrs[2].role, AttributeSpec::Role::Output);
}

TEST(Graph, AddLink)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    auto b = g.add_node(adder, "b", "control", {});

    PinRef from{ a, "out" };
    PinRef to{ b, "a" };
    auto lid = g.add_link(from, to, "float");
    EXPECT_NE(lid, invalid_link_id);
    ASSERT_EQ(g.links().size(), 1u);

    auto const* l = g.find_link(lid);
    ASSERT_NE(l, nullptr);
    EXPECT_EQ(l->from, from);
    EXPECT_EQ(l->to, to);
    EXPECT_EQ(l->data_type, "float");
}

TEST(Graph, AddLinkRejectsUnknownPin)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});

    EXPECT_EQ(g.add_link({ 9999, "out" }, { a, "a" }, "float"), invalid_link_id);
    EXPECT_EQ(g.add_link({ a, "no_such" }, { a, "a" }, "float"), invalid_link_id);
    EXPECT_TRUE(g.links().empty());
}

TEST(Graph, RemoveNodeCascadesLinks)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    auto b = g.add_node(adder, "b", "control", {});
    auto c = g.add_node(adder, "c", "control", {});

    g.add_link({ a, "out" }, { b, "a" }, "float");
    g.add_link({ b, "out" }, { c, "a" }, "float");
    g.add_link({ a, "out" }, { c, "b" }, "float");
    ASSERT_EQ(g.links().size(), 3u);

    g.remove_node(b);
    EXPECT_EQ(g.nodes().size(), 2u);
    ASSERT_EQ(g.links().size(), 1u);

    auto const& l = g.links()[0];
    EXPECT_EQ(l.from.node, a);
    EXPECT_EQ(l.to.node, c);
}

TEST(Graph, RemoveLink)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    auto b = g.add_node(adder, "b", "control", {});
    auto lid = g.add_link({ a, "out" }, { b, "a" }, "float");

    g.remove_link(lid);
    EXPECT_TRUE(g.links().empty());

    g.remove_link(lid);
    EXPECT_TRUE(g.links().empty());
}

TEST(Graph, StagesCascadeToNodes)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    g.add_node(adder, "b", "feedback", {});

    g.add_stage({ "control",  rgba{ 0xFF0000FFu } });
    g.add_stage({ "feedback", rgba{ 0x00FF00FFu } });
    ASSERT_EQ(g.stages().size(), 2u);

    g.remove_stage("control");
    EXPECT_EQ(g.stages().size(), 1u);

    auto const* na = g.find_node(a);
    ASSERT_NE(na, nullptr);
    EXPECT_TRUE(na->stage.empty());
}

TEST(Graph, ModeProfileCrud)
{
    Graph g;
    ModeProfile p;
    p.name        = "default";
    p.is_default  = true;
    p.per_node["n1"] = "enable";
    p.per_node["n2"] = "disable";
    g.add_mode_profile(p);
    ASSERT_EQ(g.mode_profiles().size(), 1u);

    ModeProfile q;
    q.name = "safety";
    q.per_node["n1"] = "disable";
    g.add_mode_profile(q);
    ASSERT_EQ(g.mode_profiles().size(), 2u);

    g.remove_mode_profile("default");
    ASSERT_EQ(g.mode_profiles().size(), 1u);
    EXPECT_EQ(g.mode_profiles()[0].name, "safety");
}

TEST(Graph, IdsAreDistinctAndMonotonic)
{
    Graph g;
    auto adder = make_adder();

    auto a = g.add_node(adder, "a", "", {});
    auto b = g.add_node(adder, "b", "", {});
    auto c = g.add_node(adder, "c", "", {});
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);
    EXPECT_GT(b, a);
    EXPECT_GT(c, b);

    auto l1 = g.add_link({ a, "out" }, { b, "a" }, "float");
    auto l2 = g.add_link({ b, "out" }, { c, "a" }, "float");
    EXPECT_NE(l1, l2);
    EXPECT_GT(l2, l1);
}
