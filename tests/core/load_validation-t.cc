#include <stdexcept>

#include <gtest/gtest.h>

#include "piper/registry.h"
#include "piper/serialize_v2.h"

#include "test_helpers.h"

using namespace piper;
using piper::fixtures::any_of_kind;
using piper::fixtures::make_simple_type;

// ---- Id validation ----

TEST(LoadValidation, NodeIdZeroIsSchemaErrorAndSkipped)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    std::string const text = R"({
        "version": 2,
        "nodes": [
            {"id": 0, "type": "Simple", "name": "zero", "stage": "", "pos": [0,0], "attrs": []}
        ],
        "links": [],
        "stages": [],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, Diagnostic::Kind::SchemaError));
    EXPECT_TRUE(loaded.graph.nodes().empty());
}

TEST(LoadValidation, NegativeNodeIdSkippedAndAllocatorStaysValid)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    // Pre-fix, -1 read as uint64 max wrapped the allocator to the
    // invalid sentinel 0; subsequently added nodes collided.
    std::string const text = R"({
        "version": 2,
        "nodes": [
            {"id": -1, "type": "Simple", "name": "neg", "stage": "", "pos": [0,0], "attrs": []},
            {"id": 3,  "type": "Simple", "name": "ok",  "stage": "", "pos": [0,0], "attrs": []}
        ],
        "links": [],
        "stages": [],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, Diagnostic::Kind::SchemaError));
    ASSERT_EQ(loaded.graph.nodes().size(), 1u);
    EXPECT_EQ(loaded.graph.nodes()[0].id, 3u);

    auto first  = loaded.graph.add_node(simple, "fresh_a", "", Point{});
    auto second = loaded.graph.add_node(simple, "fresh_b", "", Point{});
    EXPECT_NE(first,  invalid_node_id);
    EXPECT_NE(second, invalid_node_id);
    EXPECT_GT(first,  3u);
    EXPECT_NE(first,  second);
}

TEST(LoadValidation, LinkIdZeroIsSchemaErrorAndSkipped)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    std::string const text = R"({
        "version": 2,
        "nodes": [
            {"id": 1, "type": "Simple", "name": "a", "stage": "", "pos": [0,0],
             "attrs": [{"name": "out", "data_type": "float", "role": "output"}]},
            {"id": 2, "type": "Simple", "name": "b", "stage": "", "pos": [0,0],
             "attrs": [{"name": "in", "data_type": "float", "role": "input"}]}
        ],
        "links": [
            {"id": 0, "from": {"node": 1, "attr": "out"},
                      "to":   {"node": 2, "attr": "in"}, "data_type": "float"}
        ],
        "stages": [],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, Diagnostic::Kind::SchemaError));
    EXPECT_TRUE(loaded.graph.links().empty());

    auto fresh = loaded.graph.add_link({ 1, "out" }, { 2, "in" }, "float");
    EXPECT_NE(fresh, invalid_link_id);
}

TEST(LoadValidation, GraphInsertRejectsIdZero)
{
    Graph g;
    Node n;
    n.id   = invalid_node_id;
    n.type = "Simple";
    EXPECT_FALSE(g.insert_node(n));
    EXPECT_TRUE(g.nodes().empty());

    Link l;
    l.id = invalid_link_id;
    EXPECT_FALSE(g.insert_link(l));
    EXPECT_TRUE(g.links().empty());
}

// ---- Malformed-field containment: per-item SchemaError, item
// skipped, no exception escapes deserialize_bundle ----

TEST(LoadValidation, StageNameAsIntIsContained)
{
    NodeRegistry r;
    std::string const text = R"({
        "version": 2,
        "nodes": [],
        "links": [],
        "stages": [{"name": 5}],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, Diagnostic::Kind::SchemaError));
    EXPECT_TRUE(loaded.graph.stages().empty());
}

TEST(LoadValidation, ModeNameAsIntIsContained)
{
    NodeRegistry r;
    std::string const text = R"({
        "version": 2,
        "nodes": [],
        "links": [],
        "stages": [],
        "modes": [{"name": 7, "per_node": []}]
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, Diagnostic::Kind::SchemaError));
    EXPECT_TRUE(loaded.graph.mode_profiles().empty());
}

TEST(LoadValidation, AnnotationPosAsStringsIsContained)
{
    NodeRegistry r;
    std::string const text = R"({
        "version": 2,
        "nodes": [],
        "links": [],
        "stages": [],
        "modes": [],
        "annotations": [{"id": 1, "pos": ["a", "b"]}]
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, Diagnostic::Kind::SchemaError));
    EXPECT_TRUE(loaded.graph.annotations().empty());
}

TEST(LoadValidation, LabelIdAsStringIsContained)
{
    NodeRegistry r;
    std::string const text = R"({
        "version": 3,
        "nodes": [],
        "links": [],
        "stages": [],
        "modes": [],
        "labels": [{"id": "x", "kind": "in", "name": "tap"}]
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, Diagnostic::Kind::SchemaError));
    EXPECT_TRUE(loaded.graph.labels().empty());
}

// ---- Document-shape errors throw std::runtime_error ----

TEST(LoadValidation, VersionAsStringThrows)
{
    NodeRegistry r;
    EXPECT_THROW(v2::deserialize_bundle(R"({"version": "3", "pipelines": []})", r),
                 std::runtime_error);
}

TEST(LoadValidation, MissingVersionThrows)
{
    NodeRegistry r;
    EXPECT_THROW(v2::deserialize_bundle(R"({"pipelines": []})", r),
                 std::runtime_error);
}

TEST(LoadValidation, TopLevelArrayThrows)
{
    NodeRegistry r;
    EXPECT_THROW(v2::deserialize_bundle(R"([1, 2, 3])", r), std::runtime_error);
}

TEST(LoadValidation, PipelinesAsNumberThrows)
{
    NodeRegistry r;
    EXPECT_THROW(v2::deserialize_bundle(R"({"version": 3, "pipelines": 5})", r),
                 std::runtime_error);
}
