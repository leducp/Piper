#include <gtest/gtest.h>

#include "piper/registry.h"
#include "piper/serialize_v2.h"

#include "test_helpers.h"

using namespace piper;
using piper::fixtures::any_of_kind;
using piper::fixtures::make_simple_type;


TEST(LoadDiagnostic, UnknownNodeType)
{
    NodeRegistry empty;  // does not contain "Simple"
    Graph g;
    auto simple = make_simple_type();
    g.add_node(simple, "n", "", {});

    auto loaded = v2::deserialize(v2::serialize(g), empty);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::UnknownNodeType));
    // Loading still proceeds -- the node is preserved verbatim.
    ASSERT_EQ(loaded.graph.nodes().size(), 1u);
    EXPECT_EQ(loaded.graph.nodes()[0].type, "Simple");
}

TEST(LoadDiagnostic, AttributeDriftOnDataType)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    Graph g;
    g.add_node(simple, "n", "", {});
    std::string text = v2::serialize(g);

    // Now mutate the registry's spec: same attribute name, different data_type.
    NodeRegistry drifted;
    NodeType drifted_type = simple;
    drifted_type.attributes[0].data_type = "double";  // was "float"
    drifted.add(drifted_type);

    auto loaded = v2::deserialize(text, drifted);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::AttributeDrift));
}

TEST(LoadDiagnostic, AttributeMissingFromRegistry)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    Graph g;
    g.add_node(simple, "n", "", {});
    std::string text = v2::serialize(g);

    // Registry now has only one of the two attrs.
    NodeRegistry shrunk;
    NodeType shrunk_type;
    shrunk_type.type = "Simple";
    shrunk_type.attributes = {
        { "in", "float", AttributeSpec::Role::Input, "" },
    };
    shrunk.add(shrunk_type);

    auto loaded = v2::deserialize(text, shrunk);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::AttributeMissing));
}

TEST(LoadDiagnostic, LinkOrphanedAttribute)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    // Hand-built JSON with a link referencing an attr that doesn't exist.
    std::string text = R"({
        "version": 2,
        "nodes": [
            {
                "id": 1, "type": "Simple", "name": "a", "stage": "", "pos": [0, 0],
                "attrs": [
                    {"name": "in",  "data_type": "float", "role": "input"},
                    {"name": "out", "data_type": "float", "role": "output"}
                ]
            },
            {
                "id": 2, "type": "Simple", "name": "b", "stage": "", "pos": [0, 0],
                "attrs": [
                    {"name": "in",  "data_type": "float", "role": "input"},
                    {"name": "out", "data_type": "float", "role": "output"}
                ]
            }
        ],
        "links": [
            {
                "id": 1,
                "from": {"node": 1, "attr": "out"},
                "to":   {"node": 2, "attr": "ghost_attr"},
                "data_type": "float"
            }
        ],
        "stages": [],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::LinkOrphanedAttribute));
}

TEST(LoadDiagnostic, LinkOrphanedNode)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    std::string text = R"({
        "version": 2,
        "nodes": [
            {
                "id": 1, "type": "Simple", "name": "a", "stage": "", "pos": [0, 0],
                "attrs": [
                    {"name": "out", "data_type": "float", "role": "output"}
                ]
            }
        ],
        "links": [
            {
                "id": 1,
                "from": {"node": 1, "attr": "out"},
                "to":   {"node": 999, "attr": "in"},
                "data_type": "float"
            }
        ],
        "stages": [],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::LinkOrphanedNode));
}

TEST(LoadDiagnostic, LinkTypeMismatch)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    // Link declares "double" but endpoints are both "float".
    std::string text = R"({
        "version": 2,
        "nodes": [
            {
                "id": 1, "type": "Simple", "name": "a", "stage": "", "pos": [0, 0],
                "attrs": [{"name": "out", "data_type": "float", "role": "output"}]
            },
            {
                "id": 2, "type": "Simple", "name": "b", "stage": "", "pos": [0, 0],
                "attrs": [{"name": "in", "data_type": "float", "role": "input"}]
            }
        ],
        "links": [
            {
                "id": 1,
                "from": {"node": 1, "attr": "out"},
                "to":   {"node": 2, "attr": "in"},
                "data_type": "double"
            }
        ],
        "stages": [],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::LinkTypeMismatch));
}

TEST(LoadDiagnostic, OrphanModeReference)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    std::string text = R"({
        "version": 2,
        "nodes": [
            {
                "id": 1, "type": "Simple", "name": "a", "stage": "", "pos": [0, 0],
                "attrs": []
            }
        ],
        "links": [],
        "stages": [],
        "default_mode": "default",
        "modes": [
            {
                "name": "default",
                "per_node": [{"node": 9999, "label": "enable"}]
            }
        ]
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::OrphanModeReference));
}

TEST(LoadDiagnostic, UnknownStageReferenceFromNode)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    std::string text = R"({
        "version": 2,
        "nodes": [
            {
                "id": 1, "type": "Simple", "name": "a", "stage": "ghost_stage",
                "pos": [0, 0], "attrs": []
            }
        ],
        "links": [],
        "stages": [],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::UnknownStageReference));
}

TEST(LoadDiagnostic, UnknownStageReferenceFromAttribute)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    std::string text = R"({
        "version": 2,
        "nodes": [
            {
                "id": 1, "type": "Simple", "name": "a", "stage": "control",
                "pos": [0, 0],
                "attrs": [
                    {"name": "out", "data_type": "float", "role": "output",
                     "stages": ["ghost_stage"]}
                ]
            }
        ],
        "links": [],
        "stages": [{"name": "control", "color": "#FF0000FF"}],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::UnknownStageReference));
}

TEST(LoadDiagnostic, DuplicateNodeId)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    std::string text = R"({
        "version": 2,
        "nodes": [
            {"id": 1, "type": "Simple", "name": "a", "stage": "", "pos": [0,0], "attrs": []},
            {"id": 1, "type": "Simple", "name": "b", "stage": "", "pos": [0,0], "attrs": []}
        ],
        "links": [],
        "stages": [],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::DuplicateNodeId));
}

TEST(LoadDiagnostic, AttributeAdded)
{
    // Saved graph has an "in"+"out" attribute, but the registry was
    // extended with a new "extra" since the graph was saved.
    NodeRegistry r;
    NodeType extended;
    extended.type = "Simple";
    extended.attributes = {
        { "in",    "float", AttributeSpec::Role::Input,  ""    },
        { "out",   "float", AttributeSpec::Role::Output, ""    },
        { "extra", "float", AttributeSpec::Role::Member, "0.0" },
    };
    r.add(extended);

    std::string text = R"({
        "version": 2,
        "nodes": [
            {
                "id": 1, "type": "Simple", "name": "n", "stage": "", "pos": [0, 0],
                "attrs": [
                    {"name": "in",  "data_type": "float", "role": "input"},
                    {"name": "out", "data_type": "float", "role": "output"}
                ]
            }
        ],
        "links": [],
        "stages": [],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::AttributeAdded));
}

TEST(LoadDiagnostic, MalformedPosFiresSchemaError)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    std::string text = R"({
        "version": 2,
        "nodes": [
            {"id": 1, "type": "Simple", "name": "n", "stage": "", "pos": [320], "attrs": []}
        ],
        "links": [],
        "stages": [],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::SchemaError));
    ASSERT_EQ(loaded.graph.nodes().size(), 1u);
    Point const expected{ 0.0f, 0.0f };
    EXPECT_EQ(loaded.graph.nodes()[0].pos, expected);
}

TEST(LoadDiagnostic, LinkTypeMismatchStillInsertsLink)
{
    // Type-mismatched links are inserted with a diagnostic. Engine
    // consumers must check diagnostics before trusting any link.
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    std::string text = R"({
        "version": 2,
        "nodes": [
            {"id": 1, "type": "Simple", "name": "a", "stage": "", "pos": [0, 0],
             "attrs": [{"name": "out", "data_type": "float", "role": "output"}]},
            {"id": 2, "type": "Simple", "name": "b", "stage": "", "pos": [0, 0],
             "attrs": [{"name": "in", "data_type": "float", "role": "input"}]}
        ],
        "links": [
            {"id": 1, "from": {"node": 1, "attr": "out"}, "to": {"node": 2, "attr": "in"},
             "data_type": "double"}
        ],
        "stages": [],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::LinkTypeMismatch));
    EXPECT_EQ(loaded.graph.links().size(), 1u);
}

TEST(LoadDiagnostic, SchemaErrorOnMissingNodeFields)
{
    NodeRegistry r;
    std::string text = R"({
        "version": 2,
        "nodes": [{"name": "noid"}],
        "links": [],
        "stages": [],
        "modes": []
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::SchemaError));
}

TEST(LoadDiagnostic, CleanGraphHasNoDiagnostics)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    Graph g;
    auto a = g.add_node(simple, "a", "", {});
    auto b = g.add_node(simple, "b", "", {});
    g.add_link({ a, "out" }, { b, "in" }, "float");

    auto loaded = v2::deserialize(v2::serialize(g), r);
    EXPECT_TRUE(loaded.diagnostics.empty());
}

// Editor opens a drift-flagged graph, mutates, saves. Verbatim-preserved
// drift fields must still serialize and reload cleanly so the next load
// produces the same diagnostics -- silent data loss is the failure mode.
TEST(LoadDiagnostic, MutateAndSavePreservesDriftReferences)
{
    NodeRegistry r;
    auto simple = make_simple_type();
    r.add(simple);

    // First load: a graph that references a stage not in stages[].
    std::string text_v1 = R"({
        "version": 2,
        "nodes": [
            {"id": 1, "type": "Simple", "name": "a", "stage": "ghost",
             "pos": [0, 0], "attrs": [
                {"name": "out", "data_type": "float", "role": "output"}
             ]}
        ],
        "links": [],
        "stages": [],
        "modes": []
    })";

    auto first_load = v2::deserialize(text_v1, r);
    EXPECT_TRUE(any_of_kind(first_load.diagnostics, DiagnosticKind::UnknownStageReference));
    ASSERT_EQ(first_load.graph.nodes().size(), 1u);
    EXPECT_EQ(first_load.graph.nodes()[0].stage, "ghost");

    // Edit the graph: rename the node. Re-serialize.
    first_load.graph.rename_node(first_load.graph.nodes()[0].id, "renamed");
    std::string text_v2 = v2::serialize(first_load.graph);

    // Reload: diagnostic still fires (verbatim preserved), node still
    // references "ghost" stage, name change persisted.
    auto second_load = v2::deserialize(text_v2, r);
    EXPECT_TRUE(any_of_kind(second_load.diagnostics, DiagnosticKind::UnknownStageReference));
    ASSERT_EQ(second_load.graph.nodes().size(), 1u);
    EXPECT_EQ(second_load.graph.nodes()[0].stage, "ghost");
    EXPECT_EQ(second_load.graph.nodes()[0].name,  "renamed");
}
