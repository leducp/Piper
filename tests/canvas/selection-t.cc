#include <gtest/gtest.h>

#include <vector>

#include "piper/canvas/ids.h"
#include "piper/canvas/selection.h"

using namespace piper::canvas;

TEST(Selection, DefaultIsEmpty)
{
    Selection s;
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.size(), 0u);
    EXPECT_FALSE(s.contains(NodeId{1}));
}

TEST(Selection, AddInsertsAndReportsChange)
{
    Selection s;
    EXPECT_TRUE(s.add(NodeId{1}));
    EXPECT_TRUE(s.contains(NodeId{1}));
    EXPECT_EQ(s.size(), 1u);

    EXPECT_FALSE(s.add(NodeId{1}));   // duplicate — no change
    EXPECT_EQ(s.size(), 1u);

    EXPECT_TRUE(s.add(NodeId{2}));
    EXPECT_EQ(s.size(), 2u);
}

TEST(Selection, RemoveErasesAndReportsChange)
{
    Selection s;
    s.add(NodeId{1});
    s.add(NodeId{2});

    EXPECT_TRUE(s.remove(NodeId{1}));
    EXPECT_FALSE(s.contains(NodeId{1}));
    EXPECT_TRUE(s.contains(NodeId{2}));

    EXPECT_FALSE(s.remove(NodeId{99}));   // not present — no change
}

TEST(Selection, ToggleAddsThenRemoves)
{
    Selection s;
    EXPECT_TRUE(s.toggle(NodeId{1}));
    EXPECT_TRUE(s.contains(NodeId{1}));

    EXPECT_TRUE(s.toggle(NodeId{1}));
    EXPECT_FALSE(s.contains(NodeId{1}));
}

TEST(Selection, ClearOnlyChangesIfNonEmpty)
{
    Selection s;
    EXPECT_FALSE(s.clear());          // already empty

    s.add(NodeId{1});
    EXPECT_TRUE(s.clear());
    EXPECT_TRUE(s.empty());
}

TEST(Selection, SetReplacesContentsAndReportsChangeSemantics)
{
    Selection s;
    std::vector<NodeId> const ids_a{ NodeId{1}, NodeId{2} };
    EXPECT_TRUE(s.set(ids_a));
    EXPECT_EQ(s.size(), 2u);

    // Same contents → no change.
    EXPECT_FALSE(s.set(ids_a));

    std::vector<NodeId> const ids_b{ NodeId{2}, NodeId{1} };
    EXPECT_TRUE(s.set(ids_b));        // order matters → reported as change

    std::vector<NodeId> const empty;
    EXPECT_TRUE(s.set(empty));
    EXPECT_TRUE(s.empty());
}

TEST(Selection, IdsSpanReflectsCurrentContents)
{
    Selection s;
    s.add(NodeId{10});
    s.add(NodeId{20});
    auto const ids = s.ids();
    ASSERT_EQ(ids.size(), 2u);
    EXPECT_EQ(ids[0], NodeId{10});
    EXPECT_EQ(ids[1], NodeId{20});
}
