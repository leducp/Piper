#include <gtest/gtest.h>

#include "piper/builtin_nodes.h"
#include "piper/migrate/v1_reader.h"
#include "piper/registry.h"

using namespace piper;

namespace
{
    NodeRegistry default_registry()
    {
        NodeRegistry r;
        register_builtin_nodes(r);
        return r;
    }

    // Returns the named attribute's value on the named node, or "" if
    // either is missing.
    std::string attr_value(Graph const& g,
                            std::string_view node_name,
                            std::string_view attr_name)
    {
        for (auto const& n : g.nodes())
        {
            if (n.name == node_name)
            {
                for (auto const& a : n.attrs)
                {
                    if (a.name == attr_name)
                    {
                        return a.value;
                    }
                }
            }
        }
        return std::string{};
    }
}

TEST(V1Reader, MinimalGraphProducesOnePipeline)
{
    auto r = default_registry();
    std::string text = R"({
        "demo": {
            "Nodes": {
                "src":  { "type": "constant<float>", "stage": "control", "value": "1.5" },
                "sink": { "type": "probe<float>",    "stage": "control" }
            },
            "Links": [
                { "from": "src", "out": "out", "to": "sink", "in": "in", "type": "float" }
            ]
        }
    })";

    auto bundle = migrate::read_v1(text, r);
    ASSERT_EQ(bundle.pipelines.size(), 1u);
    EXPECT_EQ(bundle.pipelines[0].name, "demo");
    auto const& g = bundle.pipelines[0].graph;
    ASSERT_EQ(g.nodes().size(), 2u);
    ASSERT_EQ(g.links().size(), 1u);

    // Member values are preserved verbatim as strings; typing is
    // applied at the V2 serialize step.
    EXPECT_EQ(attr_value(g, "src", "value"), "1.5");
}

TEST(V1Reader, MultiplePipelinesEachBecomeAnEntry)
{
    auto r = default_registry();
    std::string text = R"({
        "alpha": { "Nodes": { "n": { "type": "probe<float>", "stage": "" } }, "Links": [] },
        "beta":  { "Nodes": { "m": { "type": "probe<int>",   "stage": "" } }, "Links": [] }
    })";

    auto bundle = migrate::read_v1(text, r);
    ASSERT_EQ(bundle.pipelines.size(), 2u);
    // Top-level object iteration order is implementation-defined, so
    // verify both pipelines exist by name.
    bool saw_alpha = false;
    bool saw_beta  = false;
    for (auto const& p : bundle.pipelines)
    {
        if (p.name == "alpha") { saw_alpha = true; }
        if (p.name == "beta")  { saw_beta  = true; }
    }
    EXPECT_TRUE(saw_alpha);
    EXPECT_TRUE(saw_beta);
}

TEST(V1Reader, UnknownNodeTypeFiresDiagnostic)
{
    auto r = default_registry();
    std::string text = R"({
        "demo": {
            "Nodes": { "x": { "type": "not_a_real_type", "stage": "" } },
            "Links": []
        }
    })";

    auto bundle = migrate::read_v1(text, r);
    ASSERT_EQ(bundle.pipelines.size(), 1u);
    auto const& diags = bundle.pipelines[0].diagnostics;
    bool found = false;
    for (auto const& d : diags)
    {
        if (d.kind == DiagnosticKind::UnknownNodeType)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    // Node was skipped, no graph entry created.
    EXPECT_TRUE(bundle.pipelines[0].graph.nodes().empty());
}

TEST(V1Reader, OrphanLinkFiresDiagnosticAndIsDropped)
{
    auto r = default_registry();
    std::string text = R"({
        "demo": {
            "Nodes": { "src": { "type": "constant<float>", "stage": "" } },
            "Links": [
                { "from": "src", "out": "out", "to": "ghost", "in": "in", "type": "float" }
            ]
        }
    })";

    auto bundle = migrate::read_v1(text, r);
    ASSERT_EQ(bundle.pipelines.size(), 1u);
    auto const& diags = bundle.pipelines[0].diagnostics;
    bool found = false;
    for (auto const& d : diags)
    {
        if (d.kind == DiagnosticKind::LinkOrphanedNode)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
    EXPECT_TRUE(bundle.pipelines[0].graph.links().empty());
}

TEST(V1Reader, OrphanLinkAttributeFiresDiagnostic)
{
    auto r = default_registry();
    std::string text = R"({
        "demo": {
            "Nodes": {
                "src":  { "type": "constant<float>", "stage": "" },
                "sink": { "type": "probe<float>",    "stage": "" }
            },
            "Links": [
                { "from": "src", "out": "no_such_pin", "to": "sink", "in": "in", "type": "float" }
            ]
        }
    })";

    auto bundle = migrate::read_v1(text, r);
    auto const& diags = bundle.pipelines[0].diagnostics;
    bool found = false;
    for (auto const& d : diags)
    {
        if (d.kind == DiagnosticKind::LinkOrphanedAttribute)
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(V1Reader, MalformedJsonThrows)
{
    auto r = default_registry();
    EXPECT_THROW(migrate::read_v1("{not valid", r), std::runtime_error);
}

TEST(V1Reader, NonObjectTopLevelThrows)
{
    auto r = default_registry();
    EXPECT_THROW(migrate::read_v1("[1, 2, 3]", r), std::runtime_error);
}

TEST(V1Reader, ReservedAttributesNotTreatedAsValues)
{
    // V1 stored "type" and "stage" as node-level fields. Reading them
    // back as attribute values would surface a spurious AttributeMissing
    // diagnostic for either name -- ensure the reader skips them.
    auto r = default_registry();
    std::string text = R"({
        "demo": {
            "Nodes": {
                "n": { "type": "constant<float>", "stage": "control", "value": "0.0" }
            },
            "Links": []
        }
    })";

    auto bundle = migrate::read_v1(text, r);
    auto const& diags = bundle.pipelines[0].diagnostics;
    for (auto const& d : diags)
    {
        EXPECT_NE(d.attr_name, "type");
        EXPECT_NE(d.attr_name, "stage");
    }
}
