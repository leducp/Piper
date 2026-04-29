#include <gtest/gtest.h>

#include "piper/registry.h"
#include "piper/serialize_v2.h"

using namespace piper;

namespace
{
    NodeType make_typed_value_type()
    {
        NodeType nt;
        nt.type = "Tunable";
        nt.attributes.push_back({ "in",     "float",  AttributeSpec::Role::Input,  "" });
        nt.attributes.push_back({ "out",    "float",  AttributeSpec::Role::Output, "" });
        nt.attributes.push_back({ "gain",   "float",  AttributeSpec::Role::Member, "" });
        nt.attributes.push_back({ "count",  "int",    AttributeSpec::Role::Member, "" });
        nt.attributes.push_back({ "active", "bool",   AttributeSpec::Role::Member, "" });
        nt.attributes.push_back({ "tag",    "string", AttributeSpec::Role::Member, "" });
        nt.attributes.push_back({ "shape",  "vec3",   AttributeSpec::Role::Member, "" });
        return nt;
    }
}

TEST(FormatPass, MemberValuesEncodeByDataType)
{
    NodeRegistry r;
    r.add(make_typed_value_type());

    Graph g;
    auto id = g.add_node(make_typed_value_type(), "n", "", Point{});
    g.set_attr_value(id, "gain",   "0.5");
    g.set_attr_value(id, "count",  "7");
    g.set_attr_value(id, "active", "true");
    g.set_attr_value(id, "tag",    "main");
    g.set_attr_value(id, "shape",  "[1, 2, 3]");

    std::string out = v2::serialize(g);

    // Numerics and bools become native JSON; strings stay quoted; the
    // unrecognized vec3 type still embeds verbatim as a JSON string so
    // user content survives the roundtrip.
    EXPECT_NE(out.find("\"value\": 0.5"),    std::string::npos);
    EXPECT_NE(out.find("\"value\": 7"),      std::string::npos);
    EXPECT_NE(out.find("\"value\": true"),   std::string::npos);
    EXPECT_NE(out.find("\"value\": \"main\""), std::string::npos);
    EXPECT_NE(out.find("\"value\": \"[1, 2, 3]\""), std::string::npos);

    auto loaded = v2::deserialize(out, r);
    EXPECT_TRUE(loaded.diagnostics.empty());
    Node const* n = loaded.graph.find_node(id);
    ASSERT_NE(n, nullptr);

    auto value = [&](char const* name) -> std::string
    {
        for (auto const& a : n->attrs)
        {
            if (a.name == name)
            {
                return a.value;
            }
        }
        return std::string{};
    };
    EXPECT_EQ(value("gain"),   "0.5");
    EXPECT_EQ(value("count"),  "7");
    EXPECT_EQ(value("active"), "true");
    EXPECT_EQ(value("tag"),    "main");
    EXPECT_EQ(value("shape"),  "[1, 2, 3]");
}

TEST(FormatPass, NumericValueWithUnparseableFallsBackToString)
{
    NodeRegistry r;
    r.add(make_typed_value_type());

    Graph g;
    auto id = g.add_node(make_typed_value_type(), "n", "", Point{});
    g.set_attr_value(id, "gain", "{{templated}}");

    std::string out = v2::serialize(g);
    EXPECT_NE(out.find("\"value\": \"{{templated}}\""), std::string::npos);

    auto loaded = v2::deserialize(out, r);
    Node const* n = loaded.graph.find_node(id);
    ASSERT_NE(n, nullptr);
    for (auto const& a : n->attrs)
    {
        if (a.name == "gain")
        {
            EXPECT_EQ(a.value, "{{templated}}");
        }
    }
}

TEST(FormatPass, DefaultModeTopLevelRoundTrips)
{
    NodeRegistry r;
    r.add(make_typed_value_type());

    Graph g;
    auto id = g.add_node(make_typed_value_type(), "n", "", Point{});
    ModeProfile a;
    a.name        = "alpha";
    a.per_node[id] = "enable";
    ModeProfile b;
    b.name        = "beta";
    b.per_node[id] = "disable";
    g.add_mode_profile(a);
    g.add_mode_profile(b);
    g.set_default_mode_name("beta");

    std::string out = v2::serialize(g);
    EXPECT_NE(out.find("\"default_mode\": \"beta\""), std::string::npos);
    EXPECT_EQ(out.find("\"is_default\""), std::string::npos);

    auto loaded = v2::deserialize(out, r);
    EXPECT_EQ(loaded.graph.default_mode_name(), "beta");
}

TEST(FormatPass, DefaultModePreservedEvenWhenProfileMissing)
{
    NodeRegistry r;

    std::string text = R"({
        "version": 2,
        "default_mode": "vanished",
        "modes": [
            {"name": "still_here", "per_node": []}
        ]
    })";

    auto loaded = v2::deserialize(text, r);
    EXPECT_EQ(loaded.graph.default_mode_name(), "vanished");
}

TEST(FormatPass, MetaBlockRoundTrips)
{
    NodeRegistry r;
    Graph g;
    g.meta()["author"]      = "phil";
    g.meta()["description"] = "torque feedback prototype";

    auto loaded = v2::deserialize(v2::serialize(g), r);
    EXPECT_EQ(loaded.graph.meta().size(), 2u);
    EXPECT_EQ(loaded.graph.meta().at("author"),      "phil");
    EXPECT_EQ(loaded.graph.meta().at("description"), "torque feedback prototype");
}

TEST(FormatPass, EmptyMetaIsOmitted)
{
    Graph g;
    std::string out = v2::serialize(g);
    EXPECT_EQ(out.find("\"meta\""), std::string::npos);
}
