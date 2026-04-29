#include <gtest/gtest.h>

#include "piper/graph.h"
#include "piper/registry.h"

#include "test_helpers.h"

using namespace piper;
using piper::fixtures::make_adder;

// ---- NodeRegistry ----

TEST(NodeRegistry, EmptyOnConstruction)
{
    NodeRegistry r;
    EXPECT_TRUE(r.empty());
    EXPECT_EQ(r.size(), 0u);
    EXPECT_TRUE(r.all().empty());
    EXPECT_EQ(r.find("anything"), nullptr);
}

TEST(NodeRegistry, Add)
{
    NodeRegistry r;
    EXPECT_TRUE(r.add(make_adder()));
    EXPECT_EQ(r.size(), 1u);

    EXPECT_FALSE(r.add(make_adder()));
    EXPECT_EQ(r.size(), 1u);
}

TEST(NodeRegistry, Find)
{
    NodeRegistry r;
    r.add(make_adder());

    auto const* nt = r.find("add");
    ASSERT_NE(nt, nullptr);
    EXPECT_EQ(nt->type, "add");
    EXPECT_EQ(nt->attributes.size(), 4u);

    EXPECT_EQ(r.find("sub"), nullptr);
}

// ---- Node ----

TEST(Node, FindAttrOnEmptyAttrsReturnsNullptr)
{
    Node n;
    EXPECT_EQ(n.find_attr("anything"), nullptr);
}

TEST(Node, FindAttrFindsByName)
{
    Node n;
    n.attrs.push_back({ "a",   "float", AttributeSpec::Role::Input,  "", {} });
    n.attrs.push_back({ "out", "float", AttributeSpec::Role::Output, "", {} });

    auto const* a = n.find_attr("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->name, "a");
    EXPECT_EQ(a->role, AttributeSpec::Role::Input);

    EXPECT_EQ(n.find_attr("missing"), nullptr);
}

// ---- Graph: add / find ----

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
    EXPECT_EQ(n->attrs[3].value, "1.0");
    EXPECT_EQ(n->attrs[0].role, AttributeSpec::Role::Input);
    EXPECT_EQ(n->attrs[2].role, AttributeSpec::Role::Output);
}

TEST(Graph, FindNodeWithInvalidIdReturnsNullptr)
{
    Graph g;
    EXPECT_EQ(g.find_node(invalid_node_id), nullptr);
    EXPECT_EQ(g.find_node(9999), nullptr);
}

TEST(Graph, FindLinkWithInvalidIdReturnsNullptr)
{
    Graph g;
    EXPECT_EQ(g.find_link(invalid_link_id), nullptr);
    EXPECT_EQ(g.find_link(9999), nullptr);
}

// ---- Graph: links ----

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
    EXPECT_EQ(g.links()[0].from.node, a);
    EXPECT_EQ(g.links()[0].to.node, c);
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

TEST(Graph, RemoveLinkUnknownIsNoop)
{
    Graph g;
    g.remove_link(9999);
    EXPECT_TRUE(g.links().empty());
}

// ---- Graph: stages ----

TEST(Graph, AddStage)
{
    Graph g;
    EXPECT_TRUE(g.add_stage({ "control",  rgba{ 0xFF0000FFu } }));
    EXPECT_TRUE(g.add_stage({ "feedback", rgba{ 0x00FF00FFu } }));
    EXPECT_FALSE(g.add_stage({ "control",  rgba{ 0x0000FFFFu } }));
    EXPECT_EQ(g.stages().size(), 2u);
}

TEST(Graph, RemoveStageDoesNotCascadeToNodes)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    g.add_stage({ "control", rgba{ 0xFF0000FFu } });

    g.remove_stage("control");
    EXPECT_TRUE(g.stages().empty());

    auto const* na = g.find_node(a);
    ASSERT_NE(na, nullptr);
    // Stage label preserved verbatim -- load-time UnknownStageLabel
    // diagnostic is responsible for surfacing the dangling reference.
    EXPECT_EQ(na->stage, "control");
}

TEST(Graph, RemoveStageUnknownIsNoop)
{
    Graph g;
    g.add_stage({ "control", rgba{ 0xFF0000FFu } });
    g.remove_stage("nope");
    EXPECT_EQ(g.stages().size(), 1u);
}

// ---- Graph: mode profiles ----

TEST(Graph, AddModeProfile)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});

    ModeProfile p;
    p.name        = "default";
    p.per_node[a] = "enable";
    EXPECT_TRUE(g.add_mode_profile(p));

    ModeProfile dup;
    dup.name = "default";
    EXPECT_FALSE(g.add_mode_profile(dup));
    EXPECT_EQ(g.mode_profiles().size(), 1u);
}

TEST(Graph, RemoveModeProfile)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});

    ModeProfile p;
    p.name        = "default";
    p.per_node[a] = "enable";
    g.add_mode_profile(p);

    ModeProfile q;
    q.name        = "safety";
    q.per_node[a] = "disable";
    g.add_mode_profile(q);

    g.remove_mode_profile("default");
    ASSERT_EQ(g.mode_profiles().size(), 1u);
    EXPECT_EQ(g.mode_profiles()[0].name, "safety");
}

TEST(Graph, RemoveModeProfileUnknownIsNoop)
{
    Graph g;
    g.remove_mode_profile("nope");
    EXPECT_TRUE(g.mode_profiles().empty());
}

// ---- Graph: ID monotonicity ----

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

// ---- Graph: insert_node / insert_link / reserve_ids_above ----

TEST(Graph, InsertNodePreservesId)
{
    Graph g;
    Node n;
    n.id   = 42;
    n.type = "Custom";
    n.name = "preserved";
    EXPECT_TRUE(g.insert_node(n));

    auto const* found = g.find_node(42);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->name, "preserved");
}

TEST(Graph, InsertNodeRejectsDuplicateId)
{
    Graph g;
    Node n;
    n.id = 5;
    EXPECT_TRUE(g.insert_node(n));
    EXPECT_FALSE(g.insert_node(n));
}

TEST(Graph, InsertNodeBumpsNextIdCounter)
{
    Graph g;
    auto adder = make_adder();

    Node n;
    n.id = 100;
    g.insert_node(n);

    auto fresh = g.add_node(adder, "fresh", "", {});
    EXPECT_GT(fresh, 100u);
}

TEST(Graph, InsertLinkPreservesId)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "", {});
    auto b = g.add_node(adder, "b", "", {});

    Link l;
    l.id        = 77;
    l.from      = { a, "out" };
    l.to        = { b, "a" };
    l.data_type = "float";
    EXPECT_TRUE(g.insert_link(l));

    auto const* found = g.find_link(77);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->data_type, "float");
}

TEST(Graph, InsertLinkRejectsDuplicateAndUnresolved)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "", {});
    auto b = g.add_node(adder, "b", "", {});

    Link l;
    l.id   = 1;
    l.from = { a, "out" };
    l.to   = { b, "a" };
    EXPECT_TRUE(g.insert_link(l));
    EXPECT_FALSE(g.insert_link(l));

    Link bad;
    bad.id   = 2;
    bad.from = { 9999, "out" };
    bad.to   = { b, "a" };
    EXPECT_FALSE(g.insert_link(bad));
}

TEST(Graph, ReserveIdsAbove)
{
    Graph g;
    auto adder = make_adder();

    g.reserve_ids_above(50, 70);
    auto n = g.add_node(adder, "n", "", {});
    auto a = g.add_node(adder, "a", "", {});
    auto b = g.add_node(adder, "b", "", {});
    EXPECT_GT(n, 50u);

    auto lid = g.add_link({ a, "out" }, { b, "a" }, "float");
    EXPECT_GT(lid, 70u);

    // Lower values are no-ops; counter doesn't go backwards.
    g.reserve_ids_above(0, 0);
    auto n2 = g.add_node(adder, "n2", "", {});
    EXPECT_GT(n2, n);
}

// ---- Graph: per-attribute and per-node mutators ----

TEST(Graph, MoveNode)
{
    Graph g;
    auto adder = make_adder();
    auto id = g.add_node(adder, "n", "", { 0.0f, 0.0f });

    EXPECT_TRUE(g.move_node(id, { 100.0f, 200.0f }));
    Point const expected{ 100.0f, 200.0f };
    EXPECT_EQ(g.find_node(id)->pos, expected);

    EXPECT_FALSE(g.move_node(9999, { 0.0f, 0.0f }));
}

TEST(Graph, RenameNode)
{
    Graph g;
    auto adder = make_adder();
    auto id = g.add_node(adder, "old", "", {});

    EXPECT_TRUE(g.rename_node(id, "new"));
    EXPECT_EQ(g.find_node(id)->name, "new");

    EXPECT_FALSE(g.rename_node(9999, "anything"));
}

TEST(Graph, SetNodeStage)
{
    Graph g;
    auto adder = make_adder();
    auto id = g.add_node(adder, "n", "control", {});

    EXPECT_TRUE(g.set_node_stage(id, "feedback"));
    EXPECT_EQ(g.find_node(id)->stage, "feedback");

    EXPECT_FALSE(g.set_node_stage(9999, "anything"));
}

TEST(Graph, SetAttrValue)
{
    Graph g;
    auto adder = make_adder();
    auto id = g.add_node(adder, "n", "", {});

    EXPECT_TRUE(g.set_attr_value(id, "k", "2.5"));
    EXPECT_EQ(g.find_node(id)->find_attr("k")->value, "2.5");

    EXPECT_FALSE(g.set_attr_value(id, "no_such", "x"));
    EXPECT_FALSE(g.set_attr_value(9999, "k", "x"));
}

TEST(Graph, SetAttrStages)
{
    Graph g;
    auto adder = make_adder();
    auto id = g.add_node(adder, "n", "", {});

    std::vector<std::string> const stages{ "control", "feedback" };
    EXPECT_TRUE(g.set_attr_stages(id, "out", stages));
    EXPECT_EQ(g.find_node(id)->find_attr("out")->stages, stages);

    EXPECT_FALSE(g.set_attr_stages(id, "no_such", {}));
    EXPECT_FALSE(g.set_attr_stages(9999, "out", {}));
}
