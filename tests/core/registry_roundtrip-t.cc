#include <gtest/gtest.h>

#include <algorithm>

#include "piper/serialize_v2.h"

using namespace piper;

namespace
{
    NodeType make_pid()
    {
        NodeType nt;
        nt.type     = "PID";
        nt.library  = "control";
        nt.category = "control";
        nt.help     = "Proportional-integral-derivative controller";
        nt.attributes = {
            { "setpoint", "float", AttributeSpec::Role::Input,  ""    },
            { "measured", "float", AttributeSpec::Role::Input,  ""    },
            { "out",      "float", AttributeSpec::Role::Output, ""    },
            { "kp",       "float", AttributeSpec::Role::Member, "1.0" },
            { "ki",       "float", AttributeSpec::Role::Member, "0.0" },
            { "kd",       "float", AttributeSpec::Role::Member, "0.0" },
        };
        return nt;
    }

    NodeType make_bus()
    {
        NodeType nt;
        nt.type    = "Bus";
        nt.library = "control";
        nt.attributes = {
            { "torque_cmd",  "vec3", AttributeSpec::Role::Output, "" },
            { "torque_meas", "vec3", AttributeSpec::Role::Input,  "" },
        };
        return nt;
    }

    bool any_of_kind(std::vector<Diagnostic> const& diags, DiagnosticKind k)
    {
        for (auto const& d : diags)
        {
            if (d.kind == k)
            {
                return true;
            }
        }
        return false;
    }

}

TEST(RegistryRoundTrip, EmptyRoundTrips)
{
    NodeRegistry empty;
    auto loaded = v2::deserialize_registry(v2::serialize_registry(empty));
    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_TRUE(loaded.registry.empty());
}

TEST(RegistryRoundTrip, PreservesAllFields)
{
    NodeRegistry original;
    original.add(make_pid());
    original.add(make_bus());

    auto loaded = v2::deserialize_registry(v2::serialize_registry(original));
    EXPECT_TRUE(loaded.diagnostics.empty());
    ASSERT_EQ(loaded.registry.size(), 2u);

    auto const* pid = loaded.registry.find("PID");
    ASSERT_NE(pid, nullptr);
    EXPECT_EQ(pid->library,  "control");
    EXPECT_EQ(pid->category, "control");
    EXPECT_EQ(pid->help,     "Proportional-integral-derivative controller");
    ASSERT_EQ(pid->attributes.size(), 6u);
    EXPECT_EQ(pid->attributes[0].name, "setpoint");
    EXPECT_EQ(pid->attributes[3].default_value, "1.0");
    EXPECT_EQ(pid->attributes[3].role, AttributeSpec::Role::Member);
    EXPECT_EQ(pid->attributes[2].role, AttributeSpec::Role::Output);

    auto const* bus = loaded.registry.find("Bus");
    ASSERT_NE(bus, nullptr);
    EXPECT_EQ(bus->library, "control");
    EXPECT_TRUE(bus->category.empty());  // omitted on serialize, defaults on read
}

TEST(RegistryRoundTrip, OptionalFieldsOmittedWhenEmpty)
{
    NodeRegistry r;
    NodeType minimal;
    minimal.type = "Minimal";
    minimal.attributes = {
        { "in", "float", AttributeSpec::Role::Input, "" },
    };
    r.add(minimal);

    std::string text = v2::serialize_registry(r);
    // No library/category/help/default_value fields in the JSON.
    EXPECT_EQ(text.find("library"),       std::string::npos);
    EXPECT_EQ(text.find("category"),      std::string::npos);
    EXPECT_EQ(text.find("help"),          std::string::npos);
    EXPECT_EQ(text.find("default_value"), std::string::npos);
}

TEST(RegistryDeserialize, DuplicateTypeNameDiagnostic)
{
    std::string text = R"({
        "version": 2,
        "types": [
            {"type": "X", "attributes": []},
            {"type": "X", "attributes": []}
        ]
    })";
    auto loaded = v2::deserialize_registry(text);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::DuplicateTypeName));
    EXPECT_EQ(loaded.registry.size(), 1u);
}

TEST(RegistryDeserialize, MissingTypeFieldSchemaError)
{
    std::string text = R"({
        "version": 2,
        "types": [{"library": "anonymous"}]
    })";
    auto loaded = v2::deserialize_registry(text);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::SchemaError));
    EXPECT_TRUE(loaded.registry.empty());
}

TEST(RegistryDeserialize, UnknownRoleSchemaError)
{
    std::string text = R"({
        "version": 2,
        "types": [
            {
                "type": "T",
                "attributes": [
                    {"name": "x", "data_type": "float", "role": "bogus"}
                ]
            }
        ]
    })";
    auto loaded = v2::deserialize_registry(text);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::SchemaError));
    // Type is still registered, just without the bad attribute.
    auto const* t = loaded.registry.find("T");
    ASSERT_NE(t, nullptr);
    EXPECT_TRUE(t->attributes.empty());
}

TEST(RegistryDeserialize, MissingAttributeFieldSchemaError)
{
    std::string text = R"({
        "version": 2,
        "types": [
            {"type": "T", "attributes": [{"name": "x"}]}
        ]
    })";
    auto loaded = v2::deserialize_registry(text);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::SchemaError));
}

TEST(RegistryDeserialize, ThrowsOnMalformedJson)
{
    EXPECT_THROW(v2::deserialize_registry("not json"),    std::runtime_error);
    EXPECT_THROW(v2::deserialize_registry("{}"),          std::runtime_error);  // version 0
}

TEST(RegistryDeserialize, ThrowsOnUnsupportedVersion)
{
    EXPECT_THROW(v2::deserialize_registry(R"({"version": 1})"), std::runtime_error);
    EXPECT_THROW(v2::deserialize_registry(R"({"version": 99})"), std::runtime_error);
}

// Engine workflow: deserialize registry, then deserialize a graph that
// references types from that registry. The two serializers compose.
TEST(Registry, DeserializedRegistryUsableAsGraphContext)
{
    NodeRegistry source;
    source.add(make_bus());

    auto loaded_reg = v2::deserialize_registry(v2::serialize_registry(source));
    ASSERT_TRUE(loaded_reg.diagnostics.empty());

    Graph g;
    auto const* bus = loaded_reg.registry.find("Bus");
    ASSERT_NE(bus, nullptr);
    g.add_node(*bus, "instance", "", { 0.0f, 0.0f });

    auto graph_loaded = v2::deserialize(v2::serialize(g), loaded_reg.registry);
    EXPECT_TRUE(graph_loaded.diagnostics.empty());
    EXPECT_EQ(graph_loaded.graph.nodes().size(), 1u);
}
