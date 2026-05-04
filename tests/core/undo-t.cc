#include <gtest/gtest.h>

#include <memory>

#include "piper/command_stack.h"
#include "piper/commands.h"
#include "piper/graph.h"

using namespace piper;

NodeType make_simple()
{
    NodeType nt;
    nt.type = "Simple";
    nt.attributes = {
        { "in",  "float", AttributeSpec::Role::Input,  ""    },
        { "out", "float", AttributeSpec::Role::Output, ""    },
        { "k",   "float", AttributeSpec::Role::Member, "1.0" },
    };
    return nt;
}

// ---- CommandStack mechanics ----

TEST(CommandStack, EmptyOnConstruction)
{
    CommandStack stack;
    EXPECT_FALSE(stack.can_undo());
    EXPECT_FALSE(stack.can_redo());
    EXPECT_EQ(stack.undo_size(), 0u);
    EXPECT_EQ(stack.redo_size(), 0u);
}

TEST(CommandStack, PushAppliesAndAddsToUndo)
{
    Graph g;
    auto type = make_simple();
    CommandStack stack;

    stack.push(std::make_unique<AddNodeCommand>(type, "n", "", Point{}), g);
    EXPECT_EQ(g.nodes().size(), 1u);
    EXPECT_TRUE(stack.can_undo());
    EXPECT_FALSE(stack.can_redo());
}

TEST(CommandStack, UndoReverts)
{
    Graph g;
    auto type = make_simple();
    CommandStack stack;

    stack.push(std::make_unique<AddNodeCommand>(type, "n", "", Point{}), g);
    stack.undo(g);
    EXPECT_TRUE(g.nodes().empty());
    EXPECT_FALSE(stack.can_undo());
    EXPECT_TRUE(stack.can_redo());
}

TEST(CommandStack, RedoReapplies)
{
    Graph g;
    auto type = make_simple();
    CommandStack stack;

    stack.push(std::make_unique<AddNodeCommand>(type, "n", "", Point{}), g);
    stack.undo(g);
    stack.redo(g);
    EXPECT_EQ(g.nodes().size(), 1u);
    EXPECT_TRUE(stack.can_undo());
    EXPECT_FALSE(stack.can_redo());
}

TEST(CommandStack, NewPushClearsRedoStack)
{
    Graph g;
    auto type = make_simple();
    CommandStack stack;

    stack.push(std::make_unique<AddNodeCommand>(type, "a", "", Point{}), g);
    stack.push(std::make_unique<AddNodeCommand>(type, "b", "", Point{}), g);
    stack.undo(g);
    ASSERT_TRUE(stack.can_redo());

    stack.push(std::make_unique<AddNodeCommand>(type, "c", "", Point{}), g);
    EXPECT_FALSE(stack.can_redo());
}

TEST(CommandStack, UndoOnEmptyIsNoop)
{
    Graph g;
    CommandStack stack;
    stack.undo(g);
    EXPECT_TRUE(g.nodes().empty());
}

TEST(CommandStack, RedoOnEmptyIsNoop)
{
    Graph g;
    CommandStack stack;
    stack.redo(g);
    EXPECT_TRUE(g.nodes().empty());
}

TEST(CommandStack, ClearEmptiesBothStacks)
{
    Graph g;
    auto type = make_simple();
    CommandStack stack;

    stack.push(std::make_unique<AddNodeCommand>(type, "a", "", Point{}), g);
    stack.push(std::make_unique<AddNodeCommand>(type, "b", "", Point{}), g);
    stack.undo(g);

    stack.clear();
    EXPECT_FALSE(stack.can_undo());
    EXPECT_FALSE(stack.can_redo());
}

// ---- AddNodeCommand ----

TEST(AddNodeCommand, AddRevertReaddRestoresExactId)
{
    Graph g;
    auto type = make_simple();
    CommandStack stack;

    auto cmd = std::make_unique<AddNodeCommand>(type, "n", "control", Point{ 5, 5 });
    AddNodeCommand* cmd_ptr = cmd.get();
    stack.push(std::move(cmd), g);

    NodeId const original_id = cmd_ptr->node_id();
    ASSERT_NE(original_id, invalid_node_id);

    stack.undo(g);
    EXPECT_EQ(g.find_node(original_id), nullptr);

    stack.redo(g);
    auto const* restored = g.find_node(original_id);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->name, "n");
    EXPECT_EQ(restored->stage, "control");
}

TEST(AddNodeCommand, AttrEditsBetweenApplyAndRevertSurviveRedo)
{
    Graph g;
    auto type = make_simple();
    CommandStack stack;

    auto cmd = std::make_unique<AddNodeCommand>(type, "n", "", Point{});
    AddNodeCommand* cmd_ptr = cmd.get();
    stack.push(std::move(cmd), g);
    NodeId const id = cmd_ptr->node_id();

    // Independent edit: change attr value (not via the command stack).
    g.set_attr_value(id, "k", "9.9");

    stack.undo(g);  // captures the edit into the snapshot
    stack.redo(g);

    auto const* restored = g.find_node(id);
    ASSERT_NE(restored, nullptr);
    auto const* k = restored->find_attr("k");
    ASSERT_NE(k, nullptr);
    EXPECT_EQ(k->value, "9.9");
}

// ---- DeleteNodeCommand ----

TEST(DeleteNodeCommand, RestoresNodeAndIncidentLinks)
{
    Graph g;
    auto type = make_simple();
    auto a = g.add_node(type, "a", "", {});
    auto b = g.add_node(type, "b", "", {});
    auto c = g.add_node(type, "c", "", {});
    g.add_link({ a, "out" }, { b, "in" }, "float");
    g.add_link({ b, "out" }, { c, "in" }, "float");
    g.add_link({ a, "out" }, { c, "in" }, "float");
    ASSERT_EQ(g.links().size(), 3u);

    CommandStack stack;
    stack.push(std::make_unique<DeleteNodeCommand>(b), g);
    EXPECT_EQ(g.nodes().size(), 2u);
    EXPECT_EQ(g.links().size(), 1u);

    stack.undo(g);
    EXPECT_EQ(g.nodes().size(), 3u);
    EXPECT_EQ(g.links().size(), 3u);
    EXPECT_NE(g.find_node(b), nullptr);
}

// ---- MoveNodeCommand ----

TEST(MoveNodeCommand, ApplyRevertRoundTrip)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "", Point{ 10, 10 });
    CommandStack stack;

    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 100, 200 }), g);
    Point const moved{ 100, 200 };
    EXPECT_EQ(g.find_node(id)->pos, moved);

    stack.undo(g);
    Point const original{ 10, 10 };
    EXPECT_EQ(g.find_node(id)->pos, original);

    stack.redo(g);
    EXPECT_EQ(g.find_node(id)->pos, moved);
}

// ---- RenameNodeCommand ----

TEST(RenameNodeCommand, ApplyRevertRoundTrip)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "old", "", {});
    CommandStack stack;

    stack.push(std::make_unique<RenameNodeCommand>(id, "new"), g);
    EXPECT_EQ(g.find_node(id)->name, "new");

    stack.undo(g);
    EXPECT_EQ(g.find_node(id)->name, "old");

    stack.redo(g);
    EXPECT_EQ(g.find_node(id)->name, "new");
}

// ---- SetNodeStageCommand ----

TEST(SetNodeStageCommand, ApplyRevertRoundTrip)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "control", {});
    CommandStack stack;

    stack.push(std::make_unique<SetNodeStageCommand>(id, "feedback"), g);
    EXPECT_EQ(g.find_node(id)->stage, "feedback");

    stack.undo(g);
    EXPECT_EQ(g.find_node(id)->stage, "control");
}

// ---- CreateLinkCommand ----

TEST(CreateLinkCommand, ApplyRevertRoundTripPreservesId)
{
    Graph g;
    auto type = make_simple();
    auto a = g.add_node(type, "a", "", {});
    auto b = g.add_node(type, "b", "", {});
    CommandStack stack;

    auto cmd = std::make_unique<CreateLinkCommand>(PinRef{ a, "out" }, PinRef{ b, "in" }, "float");
    CreateLinkCommand* cmd_ptr = cmd.get();
    stack.push(std::move(cmd), g);
    LinkId const original = cmd_ptr->link_id();
    ASSERT_NE(original, invalid_link_id);

    stack.undo(g);
    EXPECT_EQ(g.find_link(original), nullptr);

    stack.redo(g);
    EXPECT_NE(g.find_link(original), nullptr);
}

// ---- DeleteLinkCommand ----

TEST(DeleteLinkCommand, RestoresExactLink)
{
    Graph g;
    auto type = make_simple();
    auto a = g.add_node(type, "a", "", {});
    auto b = g.add_node(type, "b", "", {});
    auto link_id = g.add_link({ a, "out" }, { b, "in" }, "float");
    CommandStack stack;

    stack.push(std::make_unique<DeleteLinkCommand>(link_id), g);
    EXPECT_EQ(g.find_link(link_id), nullptr);

    stack.undo(g);
    auto const* restored = g.find_link(link_id);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->data_type, "float");
}

// ---- SetAttributeValueCommand ----

TEST(SetAttributeValueCommand, ApplyRevertRoundTrip)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "", {});
    CommandStack stack;

    stack.push(std::make_unique<SetAttributeValueCommand>(id, "k", "5.0"), g);
    EXPECT_EQ(g.find_node(id)->find_attr("k")->value, "5.0");

    stack.undo(g);
    EXPECT_EQ(g.find_node(id)->find_attr("k")->value, "1.0");
}

// ---- SetAttributeStagesCommand ----

TEST(SetAttributeStagesCommand, ApplyRevertRoundTrip)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "", {});
    CommandStack stack;

    std::vector<std::string> const new_stages{ "control", "feedback" };
    stack.push(std::make_unique<SetAttributeStagesCommand>(id, "out", new_stages), g);
    EXPECT_EQ(g.find_node(id)->find_attr("out")->stages, new_stages);

    stack.undo(g);
    EXPECT_TRUE(g.find_node(id)->find_attr("out")->stages.empty());
}

TEST(SetNodeStageCommand, RedoRestoresNewStage)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "control", {});
    CommandStack stack;

    stack.push(std::make_unique<SetNodeStageCommand>(id, "feedback"), g);
    stack.undo(g);
    stack.redo(g);
    EXPECT_EQ(g.find_node(id)->stage, "feedback");
}

TEST(DeleteNodeCommand, NodeWithNoIncidentLinks)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "alone", "", {});
    CommandStack stack;

    stack.push(std::make_unique<DeleteNodeCommand>(id), g);
    EXPECT_TRUE(g.nodes().empty());

    stack.undo(g);
    ASSERT_NE(g.find_node(id), nullptr);
    EXPECT_TRUE(g.links().empty());
}

// ---- Composite / group coalescing ----

TEST(CommandStack, GroupCoalescesMultiplePushes)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "", Point{ 0, 0 });
    CommandStack stack;

    // Simulate a drag: many small MoveNodeCommands inside one group.
    stack.open_group();
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 10, 10 }), g);
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 20, 20 }), g);
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 30, 30 }), g);
    stack.close_group();

    EXPECT_EQ(stack.undo_size(), 1u);   // one composite, not three
    Point const final_pos{ 30, 30 };
    EXPECT_EQ(g.find_node(id)->pos, final_pos);

    stack.undo(g);
    Point const original{ 0, 0 };
    EXPECT_EQ(g.find_node(id)->pos, original);

    stack.redo(g);
    EXPECT_EQ(g.find_node(id)->pos, final_pos);
}

TEST(CommandStack, EmptyGroupIsNotPushed)
{
    Graph g;
    CommandStack stack;

    stack.open_group();
    stack.close_group();
    EXPECT_EQ(stack.undo_size(), 0u);
}

TEST(CommandStack, NestedGroupRequiresMatchingClosesToCommit)
{
    // Symmetric open/close -- only the outermost close commits.
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "", {});
    CommandStack stack;

    stack.open_group();
    stack.open_group();
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 10, 10 }), g);
    stack.close_group();
    EXPECT_EQ(stack.undo_size(), 0u);   // inner close: still inside outer group
    EXPECT_TRUE(stack.group_open());

    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 20, 20 }), g);
    stack.close_group();
    EXPECT_EQ(stack.undo_size(), 1u);   // outer close: commits
    EXPECT_FALSE(stack.group_open());
}

TEST(CommandStack, ExtraCloseGroupIsNoop)
{
    CommandStack stack;
    stack.close_group();   // no-op when not in a group
    EXPECT_FALSE(stack.group_open());
    EXPECT_EQ(stack.undo_size(), 0u);
}

TEST(CommandStack, CompositeRevertRunsInReverseOrder)
{
    // Add two nodes + a link in a group. Reverting the composite
    // must remove the link first, then the nodes.
    Graph g;
    auto type = make_simple();
    CommandStack stack;

    stack.open_group();
    auto a_cmd = std::make_unique<AddNodeCommand>(type, "a", "", Point{});
    auto* a_ptr = a_cmd.get();
    stack.push(std::move(a_cmd), g);
    NodeId const a = a_ptr->node_id();

    auto b_cmd = std::make_unique<AddNodeCommand>(type, "b", "", Point{});
    auto* b_ptr = b_cmd.get();
    stack.push(std::move(b_cmd), g);
    NodeId const b = b_ptr->node_id();

    stack.push(std::make_unique<CreateLinkCommand>(PinRef{ a, "out" }, PinRef{ b, "in" }, "float"), g);
    stack.close_group();
    ASSERT_EQ(g.nodes().size(), 2u);
    ASSERT_EQ(g.links().size(), 1u);

    // Reverse-order revert: link first, then nodes -- succeeds without
    // referencing already-deleted nodes.
    stack.undo(g);
    EXPECT_TRUE(g.nodes().empty());
    EXPECT_TRUE(g.links().empty());

    stack.redo(g);
    EXPECT_EQ(g.nodes().size(), 2u);
    EXPECT_EQ(g.links().size(), 1u);
}

TEST(CommandStack, ClearMidGroupDiscardsPending)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "", {});
    CommandStack stack;

    stack.open_group();
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 50, 50 }), g);
    EXPECT_TRUE(stack.group_open());

    stack.clear();
    EXPECT_FALSE(stack.group_open());
    EXPECT_EQ(stack.undo_size(), 0u);
}

// ---- Coalescing via try_merge ----

TEST(MoveNodeCommand, MergesWithinGroup)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "", Point{ 0, 0 });
    CommandStack stack;

    stack.open_group();
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 10, 10 }), g);
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 20, 20 }), g);
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 30, 30 }), g);
    stack.close_group();

    EXPECT_EQ(stack.undo_size(), 1u);
    Point const final_pos{ 30, 30 };
    EXPECT_EQ(g.find_node(id)->pos, final_pos);

    stack.undo(g);
    Point const original{ 0, 0 };
    EXPECT_EQ(g.find_node(id)->pos, original);

    stack.redo(g);
    EXPECT_EQ(g.find_node(id)->pos, final_pos);
}

TEST(MoveNodeCommand, DoesNotMergeAcrossDifferentNodes)
{
    Graph g;
    auto type = make_simple();
    auto a = g.add_node(type, "a", "", {});
    auto b = g.add_node(type, "b", "", {});
    CommandStack stack;

    stack.open_group();
    stack.push(std::make_unique<MoveNodeCommand>(a, Point{ 10, 10 }), g);
    stack.push(std::make_unique<MoveNodeCommand>(b, Point{ 20, 20 }), g);
    stack.push(std::make_unique<MoveNodeCommand>(a, Point{ 30, 30 }), g);
    stack.close_group();

    // The composite should hold all three: A merges only with the
    // back of the group, which alternates B between A's pushes.
    stack.undo(g);
    Point const a_orig{ 0, 0 };
    Point const b_orig{ 0, 0 };
    EXPECT_EQ(g.find_node(a)->pos, a_orig);
    EXPECT_EQ(g.find_node(b)->pos, b_orig);
}

TEST(SetAttributeValueCommand, MergesWithinGroup)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "", {});
    CommandStack stack;

    stack.open_group();
    stack.push(std::make_unique<SetAttributeValueCommand>(id, "k", "1"), g);
    stack.push(std::make_unique<SetAttributeValueCommand>(id, "k", "12"), g);
    stack.push(std::make_unique<SetAttributeValueCommand>(id, "k", "123"), g);
    stack.close_group();

    EXPECT_EQ(stack.undo_size(), 1u);
    EXPECT_EQ(g.find_node(id)->find_attr("k")->value, "123");

    stack.undo(g);
    EXPECT_EQ(g.find_node(id)->find_attr("k")->value, "1.0");   // original spec default
}

TEST(SetAttributeStagesCommand, MergesWithinGroup)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "", {});
    CommandStack stack;

    stack.open_group();
    stack.push(std::make_unique<SetAttributeStagesCommand>(
        id, "out", std::vector<std::string>{ "control" }), g);
    stack.push(std::make_unique<SetAttributeStagesCommand>(
        id, "out", std::vector<std::string>{ "control", "feedback" }), g);
    stack.close_group();

    EXPECT_EQ(stack.undo_size(), 1u);
    EXPECT_EQ(g.find_node(id)->find_attr("out")->stages.size(), 2u);

    stack.undo(g);
    EXPECT_TRUE(g.find_node(id)->find_attr("out")->stages.empty());
}

// ---- ScopedGroup ----

TEST(ScopedGroup, OpensAndClosesAutomatically)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "", Point{ 0, 0 });
    CommandStack stack;

    {
        ScopedGroup grp(stack);
        stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 10, 10 }), g);
        stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 20, 20 }), g);
    }   // ScopedGroup destructs, closes group

    EXPECT_FALSE(stack.group_open());
    EXPECT_EQ(stack.undo_size(), 1u);
}

// ---- max_undo cap ----

TEST(CommandStack, MaxUndoCapEvictsOldestEntries)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "", {});
    CommandStack stack;

    stack.set_max_undo(3);

    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 1, 1 }), g);
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 2, 2 }), g);
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 3, 3 }), g);
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 4, 4 }), g);
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 5, 5 }), g);

    EXPECT_EQ(stack.undo_size(), 3u);

    // Undo as far as possible -- only the last three entries are reachable.
    stack.undo(g);
    stack.undo(g);
    stack.undo(g);
    Point const after_three_undos{ 2, 2 };
    EXPECT_EQ(g.find_node(id)->pos, after_three_undos);
    EXPECT_FALSE(stack.can_undo());
}

TEST(CommandStack, MaxUndoZeroIsUnbounded)
{
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "", {});
    CommandStack stack;

    stack.set_max_undo(0);
    for (int i = 0; i < 50; ++i)
    {
        stack.push(std::make_unique<MoveNodeCommand>(id, Point{ float(i), 0 }), g);
    }
    EXPECT_EQ(stack.undo_size(), 50u);
}

// ---- Idempotent capture (double-apply protection) ----

TEST(MoveNodeCommand, DoubleApplyDoesNotStompOldPos)
{
    // Direct test of the optional guard: if apply is called twice
    // without an intervening revert, the second apply must not
    // overwrite the captured old_pos.
    Graph g;
    auto type = make_simple();
    auto id = g.add_node(type, "n", "", Point{ 0, 0 });

    auto cmd = std::make_unique<MoveNodeCommand>(id, Point{ 10, 10 });
    cmd->apply(g);
    cmd->apply(g);   // idempotent -- old_pos must still be (0, 0)
    cmd->revert(g);

    Point const original{ 0, 0 };
    EXPECT_EQ(g.find_node(id)->pos, original);
}

// ---- Cross-command sequence ----

TEST(CommandStack, MixedSequenceUndoRedo)
{
    Graph g;
    auto type = make_simple();
    CommandStack stack;

    auto add1 = std::make_unique<AddNodeCommand>(type, "a", "", Point{ 0, 0 });
    AddNodeCommand* add1_ptr = add1.get();
    stack.push(std::move(add1), g);
    NodeId const a = add1_ptr->node_id();

    auto add2 = std::make_unique<AddNodeCommand>(type, "b", "", Point{ 0, 0 });
    AddNodeCommand* add2_ptr = add2.get();
    stack.push(std::move(add2), g);
    NodeId const b = add2_ptr->node_id();

    stack.push(std::make_unique<CreateLinkCommand>(PinRef{ a, "out" }, PinRef{ b, "in" }, "float"), g);
    stack.push(std::make_unique<RenameNodeCommand>(a, "alpha"), g);

    EXPECT_EQ(g.nodes().size(), 2u);
    EXPECT_EQ(g.links().size(), 1u);
    EXPECT_EQ(g.find_node(a)->name, "alpha");

    // Walk backwards through history.
    stack.undo(g);
    EXPECT_EQ(g.find_node(a)->name, "a");
    stack.undo(g);
    EXPECT_EQ(g.links().size(), 0u);
    stack.undo(g);
    EXPECT_EQ(g.nodes().size(), 1u);
    stack.undo(g);
    EXPECT_EQ(g.nodes().size(), 0u);

    // And forward again.
    stack.redo(g);
    stack.redo(g);
    stack.redo(g);
    stack.redo(g);
    EXPECT_EQ(g.nodes().size(), 2u);
    EXPECT_EQ(g.links().size(), 1u);
    EXPECT_EQ(g.find_node(a)->name, "alpha");
}

// ---- Stage CRUD commands ----

TEST(StageCommands, AddRemovePreservePosition)
{
    Graph g;
    g.add_stage({ "first",  rgba{} });
    g.add_stage({ "second", rgba{} });
    g.add_stage({ "third",  rgba{} });

    CommandStack stack;
    stack.push(std::make_unique<RemoveStageCommand>("second"), g);

    ASSERT_EQ(g.stages().size(), 2u);
    EXPECT_EQ(g.stages()[0].name, "first");
    EXPECT_EQ(g.stages()[1].name, "third");

    stack.undo(g);
    ASSERT_EQ(g.stages().size(), 3u);
    EXPECT_EQ(g.stages()[0].name, "first");
    EXPECT_EQ(g.stages()[1].name, "second");   // restored at index 1
    EXPECT_EQ(g.stages()[2].name, "third");
}

TEST(StageCommands, MoveStageRevertsOrder)
{
    Graph g;
    g.add_stage({ "a", rgba{} });
    g.add_stage({ "b", rgba{} });
    g.add_stage({ "c", rgba{} });

    CommandStack stack;
    // Top-to-bottom drop ("a" dropped onto "c"): a goes after c.
    stack.push(std::make_unique<MoveStageCommand>("a", "c"), g);

    ASSERT_EQ(g.stages().size(), 3u);
    EXPECT_EQ(g.stages()[0].name, "b");
    EXPECT_EQ(g.stages()[1].name, "c");
    EXPECT_EQ(g.stages()[2].name, "a");

    stack.undo(g);
    EXPECT_EQ(g.stages()[0].name, "a");
    EXPECT_EQ(g.stages()[1].name, "b");
    EXPECT_EQ(g.stages()[2].name, "c");
}

TEST(StageCommands, MoveStageBottomToTopDropsBeforeTarget)
{
    Graph g;
    g.add_stage({ "a", rgba{} });
    g.add_stage({ "b", rgba{} });
    g.add_stage({ "c", rgba{} });

    CommandStack stack;
    // Bottom-to-top drop ("c" dropped onto "a"): c goes before a.
    stack.push(std::make_unique<MoveStageCommand>("c", "a"), g);

    ASSERT_EQ(g.stages().size(), 3u);
    EXPECT_EQ(g.stages()[0].name, "c");
    EXPECT_EQ(g.stages()[1].name, "a");
    EXPECT_EQ(g.stages()[2].name, "b");

    stack.undo(g);
    EXPECT_EQ(g.stages()[0].name, "a");
    EXPECT_EQ(g.stages()[2].name, "c");
}

TEST(StageCommands, SetStageColorRoundTrip)
{
    Graph g;
    rgba const initial = rgba::from_components(0x10, 0x20, 0x30, 0xFF);
    g.add_stage({ "render", initial });

    CommandStack stack;
    rgba const updated = rgba::from_components(0xAA, 0xBB, 0xCC, 0xFF);
    stack.push(std::make_unique<SetStageColorCommand>("render", updated), g);
    EXPECT_EQ(g.stages()[0].color, updated);

    stack.undo(g);
    EXPECT_EQ(g.stages()[0].color, initial);

    stack.redo(g);
    EXPECT_EQ(g.stages()[0].color, updated);
}

TEST(StageCommands, SetStageColorUnknownIsNoop)
{
    Graph g;
    g.add_stage({ "render", rgba{} });

    CommandStack stack;
    rgba const c = rgba::from_components(0x11, 0x22, 0x33, 0xFF);
    stack.push(std::make_unique<SetStageColorCommand>("nope", c), g);
    EXPECT_EQ(g.stages()[0].color, rgba{});
    stack.undo(g);
    EXPECT_EQ(g.stages()[0].color, rgba{});
}

// ---- Per-node note ----

TEST(SetNodeNoteCommand, ApplyRevertRoundTrip)
{
    Graph g;
    auto type = make_simple();
    auto id   = g.add_node(type, "n", "", Point{});
    CommandStack stack;

    stack.push(std::make_unique<SetNodeNoteCommand>(id, "tuned for X joint"), g);
    EXPECT_EQ(g.find_node(id)->note, "tuned for X joint");

    stack.undo(g);
    EXPECT_TRUE(g.find_node(id)->note.empty());

    stack.redo(g);
    EXPECT_EQ(g.find_node(id)->note, "tuned for X joint");
}

TEST(SetNodeNoteCommand, EmptyNoteRoundTrip)
{
    Graph g;
    auto type = make_simple();
    auto id   = g.add_node(type, "n", "", Point{});
    CommandStack stack;
    g.set_node_note(id, "initial");

    stack.push(std::make_unique<SetNodeNoteCommand>(id, ""), g);
    EXPECT_TRUE(g.find_node(id)->note.empty());

    stack.undo(g);
    EXPECT_EQ(g.find_node(id)->note, "initial");
}

// ---- Annotation CRUD ----

TEST(AnnotationCommands, AddRevertReaddRestoresExactId)
{
    Graph g;
    Annotation a;
    a.pos   = Point{ 100.0f, 200.0f };
    a.size  = Point{ 300.0f,  80.0f };
    a.text  = "hello";
    a.color = rgba::from_components(0x10, 0x20, 0x30, 0x80);

    CommandStack stack;
    auto cmd = std::make_unique<AddAnnotationCommand>(a);
    AddAnnotationCommand* raw = cmd.get();
    stack.push(std::move(cmd), g);

    AnnotationId const original_id = raw->annotation_id();
    ASSERT_NE(original_id, invalid_annotation_id);
    ASSERT_EQ(g.annotations().size(), 1u);

    stack.undo(g);
    EXPECT_TRUE(g.annotations().empty());

    stack.redo(g);
    ASSERT_EQ(g.annotations().size(), 1u);
    Annotation const* restored = g.find_annotation(original_id);
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->text,  "hello");
    EXPECT_EQ(restored->color, a.color);
    EXPECT_EQ(restored->pos,   a.pos);
    EXPECT_EQ(restored->size,  a.size);
}

TEST(AnnotationCommands, DeleteRestoresAtOriginalIndex)
{
    Graph g;
    Annotation a; a.text = "first";
    Annotation b; b.text = "second";
    Annotation c; c.text = "third";
    auto a_id = g.add_annotation(a);
    auto b_id = g.add_annotation(b);
    auto c_id = g.add_annotation(c);
    (void)a_id; (void)c_id;

    CommandStack stack;
    stack.push(std::make_unique<DeleteAnnotationCommand>(b_id), g);
    ASSERT_EQ(g.annotations().size(), 2u);
    EXPECT_EQ(g.annotations()[0].text, "first");
    EXPECT_EQ(g.annotations()[1].text, "third");

    stack.undo(g);
    ASSERT_EQ(g.annotations().size(), 3u);
    EXPECT_EQ(g.annotations()[0].text, "first");
    EXPECT_EQ(g.annotations()[1].text, "second");
    EXPECT_EQ(g.annotations()[2].text, "third");
}

TEST(AnnotationCommands, SetTextPosSizeColorRoundTrip)
{
    Graph g;
    Annotation a;
    a.pos   = Point{ 0.0f, 0.0f };
    a.size  = Point{ 100.0f, 50.0f };
    a.text  = "before";
    a.color = rgba::from_components(0xFF, 0x00, 0x00, 0xFF);
    auto id = g.add_annotation(a);

    CommandStack stack;

    stack.push(std::make_unique<SetAnnotationTextCommand>(id, "after"), g);
    EXPECT_EQ(g.find_annotation(id)->text, "after");
    stack.undo(g);
    EXPECT_EQ(g.find_annotation(id)->text, "before");
    stack.redo(g);
    EXPECT_EQ(g.find_annotation(id)->text, "after");

    Point const moved{ 250.0f, 320.0f };
    stack.push(std::make_unique<SetAnnotationPosCommand>(id, moved), g);
    EXPECT_EQ(g.find_annotation(id)->pos, moved);
    stack.undo(g);
    Point const orig_pos{ 0.0f, 0.0f };
    EXPECT_EQ(g.find_annotation(id)->pos, orig_pos);

    Point const resized{ 400.0f, 200.0f };
    stack.push(std::make_unique<SetAnnotationSizeCommand>(id, resized), g);
    EXPECT_EQ(g.find_annotation(id)->size, resized);
    stack.undo(g);
    Point const orig_size{ 100.0f, 50.0f };
    EXPECT_EQ(g.find_annotation(id)->size, orig_size);

    rgba const new_c = rgba::from_components(0x00, 0xFF, 0x00, 0x80);
    stack.push(std::make_unique<SetAnnotationColorCommand>(id, new_c), g);
    EXPECT_EQ(g.find_annotation(id)->color, new_c);
    stack.undo(g);
    rgba const orig_c = rgba::from_components(0xFF, 0x00, 0x00, 0xFF);
    EXPECT_EQ(g.find_annotation(id)->color, orig_c);
}

TEST(AnnotationCommands, MissingIdIsNoop)
{
    Graph g;
    CommandStack stack;
    stack.push(std::make_unique<SetAnnotationTextCommand>(999u, "ghost"), g);
    EXPECT_TRUE(g.annotations().empty());
    stack.undo(g);
    EXPECT_TRUE(g.annotations().empty());
}

// ---- Mode profile / per-node label commands ----

TEST(ModeCommands, RemoveModeProfileRestoresFullEntry)
{
    Graph g;
    NodeType nt = make_simple();
    auto a = g.add_node(nt, "a", "", Point{});
    auto b = g.add_node(nt, "b", "", Point{});

    ModeProfile mp;
    mp.name           = "default";
    mp.per_node[a]    = "enable";
    mp.per_node[b]    = "disable";
    g.add_mode_profile(mp);

    CommandStack stack;
    stack.push(std::make_unique<RemoveModeProfileCommand>("default"), g);
    EXPECT_TRUE(g.mode_profiles().empty());

    stack.undo(g);
    ASSERT_EQ(g.mode_profiles().size(), 1u);
    auto const& restored = g.mode_profiles().front();
    EXPECT_EQ(restored.name, "default");
    ASSERT_EQ(restored.per_node.size(), 2u);
    EXPECT_EQ(restored.per_node.at(a), "enable");
    EXPECT_EQ(restored.per_node.at(b), "disable");
}

TEST(ModeCommands, SetNodeModeLabelRestoresPrevious)
{
    Graph g;
    NodeType nt = make_simple();
    auto a = g.add_node(nt, "a", "", Point{});

    ModeProfile mp;
    mp.name        = "p";
    mp.per_node[a] = "enable";
    g.add_mode_profile(mp);

    CommandStack stack;
    stack.push(std::make_unique<SetNodeModeLabelCommand>("p", a, "disable"), g);
    EXPECT_EQ(g.mode_profiles().front().per_node.at(a), "disable");

    stack.undo(g);
    EXPECT_EQ(g.mode_profiles().front().per_node.at(a), "enable");

    stack.redo(g);
    EXPECT_EQ(g.mode_profiles().front().per_node.at(a), "disable");
}

TEST(ModeCommands, SetNodeModeLabelEmptyErases)
{
    Graph g;
    NodeType nt = make_simple();
    auto a = g.add_node(nt, "a", "", Point{});

    ModeProfile mp;
    mp.name        = "p";
    mp.per_node[a] = "neutral";
    g.add_mode_profile(mp);

    CommandStack stack;
    stack.push(std::make_unique<SetNodeModeLabelCommand>("p", a, ""), g);
    EXPECT_EQ(g.mode_profiles().front().per_node.count(a), 0u);

    stack.undo(g);
    EXPECT_EQ(g.mode_profiles().front().per_node.at(a), "neutral");
}
