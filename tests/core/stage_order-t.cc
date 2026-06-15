#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "piper/graph.h"

using namespace piper;

namespace stage_order_test
{
    Graph make_graph_abc()
    {
        Graph g;
        g.add_stage({ "a", rgba::from_components(0xFF, 0x00, 0x00, 0xFF) });
        g.add_stage({ "b", rgba::from_components(0x00, 0xFF, 0x00, 0xFF) });
        g.add_stage({ "c", rgba::from_components(0x00, 0x00, 0xFF, 0xFF) });
        return g;
    }

    std::vector<std::string> stage_names(Graph const& g)
    {
        std::vector<std::string> names;
        for (auto const& s : g.stages())
        {
            names.push_back(s.name);
        }
        return names;
    }
}

using stage_order_test::make_graph_abc;
using stage_order_test::stage_names;

TEST(StageOrder, MoveStageUpSwapsWithPredecessor)
{
    Graph g = make_graph_abc();
    EXPECT_TRUE(g.move_stage_up("b"));
    EXPECT_EQ(stage_names(g), (std::vector<std::string>{ "b", "a", "c" }));
}

TEST(StageOrder, MoveStageUpAtTopReturnsFalse)
{
    Graph g = make_graph_abc();
    EXPECT_FALSE(g.move_stage_up("a"));
    EXPECT_EQ(stage_names(g), (std::vector<std::string>{ "a", "b", "c" }));
}

TEST(StageOrder, MoveStageUpUnknownNameReturnsFalse)
{
    Graph g = make_graph_abc();
    EXPECT_FALSE(g.move_stage_up("ghost"));
}

TEST(StageOrder, MoveStageDownSwapsWithSuccessor)
{
    Graph g = make_graph_abc();
    EXPECT_TRUE(g.move_stage_down("b"));
    EXPECT_EQ(stage_names(g), (std::vector<std::string>{ "a", "c", "b" }));
}

TEST(StageOrder, MoveStageDownAtBottomReturnsFalse)
{
    Graph g = make_graph_abc();
    EXPECT_FALSE(g.move_stage_down("c"));
    EXPECT_EQ(stage_names(g), (std::vector<std::string>{ "a", "b", "c" }));
}

TEST(StageOrder, MoveStageDownUnknownNameReturnsFalse)
{
    Graph g = make_graph_abc();
    EXPECT_FALSE(g.move_stage_down("ghost"));
}

TEST(StageOrder, SetStagesOrderSkipsUnknownAndAppendsLeftovers)
{
    Graph g = make_graph_abc();
    g.set_stages_order({ "c", "ghost", "a" });
    // "ghost" skipped; leftover "b" appended in existing order.
    EXPECT_EQ(stage_names(g), (std::vector<std::string>{ "c", "a", "b" }));
}

TEST(StageOrder, SetStagesOrderEmptyKeepsAllStages)
{
    Graph g = make_graph_abc();
    g.set_stages_order({});
    EXPECT_EQ(stage_names(g), (std::vector<std::string>{ "a", "b", "c" }));
}

TEST(StageOrder, InsertStageAtClampsOutOfRangeIndex)
{
    Graph g = make_graph_abc();
    EXPECT_TRUE(g.insert_stage_at({ "z", rgba{} }, 99));
    EXPECT_EQ(stage_names(g), (std::vector<std::string>{ "a", "b", "c", "z" }));
}

TEST(StageOrder, InsertStageAtPlacesAtIndex)
{
    Graph g = make_graph_abc();
    EXPECT_TRUE(g.insert_stage_at({ "z", rgba{} }, 1));
    EXPECT_EQ(stage_names(g), (std::vector<std::string>{ "a", "z", "b", "c" }));
}

TEST(StageOrder, InsertStageAtRejectsDuplicateName)
{
    Graph g = make_graph_abc();
    EXPECT_FALSE(g.insert_stage_at({ "b", rgba{} }, 0));
    EXPECT_EQ(g.stages().size(), 3u);
}
