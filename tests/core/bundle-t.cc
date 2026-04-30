#include <gtest/gtest.h>

#include "piper/registry.h"
#include "piper/serialize_v2.h"

using namespace piper;

namespace
{
    NodeType make_dummy()
    {
        NodeType nt;
        nt.type = "Dummy";
        return nt;
    }
}

TEST(Bundle, EmptySingleSerializeRoundTrips)
{
    NodeRegistry r;
    Graph g;
    auto loaded = v2::deserialize(v2::serialize(g), r);
    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_EQ(loaded.graph.nodes().size(), 0u);
}

TEST(Bundle, SingleSerializeProducesPipelinesArray)
{
    NodeRegistry r;
    r.add(make_dummy());
    Graph g;
    g.add_node(make_dummy(), "alpha", Point{});

    std::string text = v2::serialize(g, "main");
    EXPECT_NE(text.find("\"pipelines\""), std::string::npos);
    EXPECT_NE(text.find("\"name\": \"main\""), std::string::npos);
}

TEST(Bundle, MultipleSerializeAndDeserialize)
{
    NodeRegistry r;
    r.add(make_dummy());

    Graph a;
    a.add_node(make_dummy(), "n_a", Point{});
    Graph b;
    b.add_node(make_dummy(), "n_b1", Point{});
    b.add_node(make_dummy(), "n_b2", Point{});

    std::vector<v2::PipelineRef> refs = {
        { "pipe_a", &a },
        { "pipe_b", &b },
    };
    std::string text = v2::serialize_bundle(refs);

    auto bundle = v2::deserialize_bundle(text, r);
    EXPECT_TRUE(bundle.diagnostics.empty());
    ASSERT_EQ(bundle.pipelines.size(), 2u);
    EXPECT_EQ(bundle.pipelines[0].name, "pipe_a");
    EXPECT_EQ(bundle.pipelines[0].graph.nodes().size(), 1u);
    EXPECT_EQ(bundle.pipelines[1].name, "pipe_b");
    EXPECT_EQ(bundle.pipelines[1].graph.nodes().size(), 2u);
}

TEST(Bundle, DeserializeAcceptsLegacyUnwrappedShape)
{
    // Files written before the bundle wrapping was introduced are
    // still loadable: the top-level doc is treated as one pipeline.
    NodeRegistry r;
    r.add(make_dummy());

    std::string text = R"({
        "version": 2,
        "nodes": [
            {"id": 1, "type": "Dummy", "name": "n", "stage": "", "pos": [0, 0], "attrs": []}
        ]
    })";

    auto bundle = v2::deserialize_bundle(text, r);
    ASSERT_EQ(bundle.pipelines.size(), 1u);
    EXPECT_EQ(bundle.pipelines[0].graph.nodes().size(), 1u);
    EXPECT_EQ(bundle.pipelines[0].name, "");
}

TEST(Bundle, DeserializeFromString)
{
    NodeRegistry r;
    r.add(make_dummy());
    Graph g;
    g.add_node(make_dummy(), "n", Point{});
    auto loaded = v2::deserialize(v2::serialize(g, "first"), r);
    ASSERT_EQ(loaded.graph.nodes().size(), 1u);
}

TEST(Bundle, DeserializeFirstFromMulti)
{
    NodeRegistry r;
    r.add(make_dummy());

    Graph a, b;
    a.add_node(make_dummy(), "a", Point{});
    b.add_node(make_dummy(), "b", Point{});
    std::string text = v2::serialize_bundle({{ "first", &a }, { "second", &b }});

    auto loaded = v2::deserialize(text, r);
    EXPECT_EQ(loaded.graph.nodes().size(), 1u);
    EXPECT_EQ(loaded.graph.nodes()[0].name, "a");
}
