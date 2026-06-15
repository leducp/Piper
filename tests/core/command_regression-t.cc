#include <memory>

#include <gtest/gtest.h>

#include "piper/command_stack.h"
#include "piper/commands.h"
#include "piper/graph.h"

using namespace piper;

namespace command_regression_test
{
    NodeType make_node_type()
    {
        NodeType nt;
        nt.type = "Simple";
        nt.attributes = {
            { "in",  "float", AttributeSpec::Role::Input,  "" },
            { "out", "float", AttributeSpec::Role::Output, "" },
        };
        return nt;
    }
}

using command_regression_test::make_node_type;

// Apply over an existing same-name stage is a no-op; revert must not
// delete the pre-existing stage.
TEST(CommandRegression, AddStageDuplicateRevertKeepsExisting)
{
    Graph g;
    rgba const original = rgba::from_components(0xAA, 0xBB, 0xCC, 0xFF);
    ASSERT_TRUE(g.add_stage({ "control", original }));

    CommandStack stack;
    stack.push(std::make_unique<AddStageCommand>(
                   Stage{ "control", rgba::from_components(0x01, 0x02, 0x03, 0xFF) }),
               g);
    ASSERT_EQ(g.stages().size(), 1u);

    stack.undo(g);
    ASSERT_EQ(g.stages().size(), 1u) << "pre-existing stage deleted by duplicate revert";
    EXPECT_EQ(g.stages()[0].name,  "control");
    EXPECT_EQ(g.stages()[0].color, original);
}

TEST(CommandRegression, AddModeProfileDuplicateRevertKeepsExisting)
{
    Graph g;
    ModeProfile existing;
    existing.name        = "default";
    existing.per_node[1] = "enable";
    ASSERT_TRUE(g.add_mode_profile(existing));

    CommandStack stack;
    ModeProfile dup;
    dup.name = "default";
    stack.push(std::make_unique<AddModeProfileCommand>(dup), g);
    ASSERT_EQ(g.mode_profiles().size(), 1u);

    stack.undo(g);
    ASSERT_EQ(g.mode_profiles().size(), 1u)
        << "pre-existing profile deleted by duplicate revert";
    EXPECT_EQ(g.mode_profiles()[0].name, "default");
    EXPECT_EQ(g.mode_profiles()[0].per_node, existing.per_node);
}

TEST(CommandRegression, NonDuplicateAddStageRevertRemovesStage)
{
    Graph g;
    CommandStack stack;
    stack.push(std::make_unique<AddStageCommand>(Stage{ "fresh", rgba{} }), g);
    ASSERT_EQ(g.stages().size(), 1u);
    stack.undo(g);
    EXPECT_TRUE(g.stages().empty());
}

// revision(): +1 on every push (including merged pushes), undo, redo.
TEST(CommandRegression, RevisionCountsPushUndoRedo)
{
    Graph g;
    auto type = make_node_type();
    CommandStack stack;
    EXPECT_EQ(stack.revision(), 0u);

    stack.push(std::make_unique<AddNodeCommand>(type, "n", "", Point{}), g);
    EXPECT_EQ(stack.revision(), 1u);
    NodeId const id = g.nodes()[0].id;

    // Merged pushes inside a group still bump the revision each time.
    stack.open_group();
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 1.0f, 1.0f }), g);
    EXPECT_EQ(stack.revision(), 2u);
    stack.push(std::make_unique<MoveNodeCommand>(id, Point{ 2.0f, 2.0f }), g);
    EXPECT_EQ(stack.revision(), 3u);
    stack.close_group();
    EXPECT_EQ(stack.revision(), 3u);

    stack.undo(g);
    EXPECT_EQ(stack.revision(), 4u);
    stack.redo(g);
    EXPECT_EQ(stack.revision(), 5u);
}

TEST(CommandRegression, RevisionUnchangedByEmptyUndoRedo)
{
    Graph g;
    CommandStack stack;
    stack.undo(g);
    stack.redo(g);
    EXPECT_EQ(stack.revision(), 0u);
}
