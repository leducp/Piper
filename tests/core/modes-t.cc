#include <gtest/gtest.h>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/modes.h"
#include "piper/registry.h"
#include "piper/serialize_v2.h"

using namespace piper;

namespace piper_modes_test
{
    NodeType make_drive()
    {
        NodeType nt;
        nt.type       = "drive";
        nt.attributes = { { "cmd", "float", AttributeSpec::Role::Input, "" } };
        nt.modes      = { "position", "speed", "torque" };
        return nt;
    }

    struct Fixture
    {
        NodeRegistry reg;
        Graph        g;

        Fixture()
        {
            register_builtin_nodes(reg);
            reg.add(make_drive());
            g.add_stage(Stage{ "ctl", 0xFFFFFFFFu });
        }

        Node add(std::string const& type, std::string const& name)
        {
            NodeType const* nt = reg.find(type);
            EXPECT_NE(nt, nullptr);
            NodeId const id = g.add_node(*nt, name, "ctl", Point{ 0.0f, 0.0f });
            return *g.find_node(id);
        }
    };
}

using piper_modes_test::Fixture;

TEST(Modes, TypeWithoutDeclaredModesHasEnableAndDisable)
{
    Fixture f;
    Node const n = f.add("constant<float>", "k");

    EXPECT_EQ(modes_of(n, f.reg), (std::vector<std::string>{ "enable", "disable" }));
    EXPECT_TRUE(accepts_mode(n, f.reg, "enable"));
    EXPECT_TRUE(accepts_mode(n, f.reg, "disable"));
    EXPECT_FALSE(accepts_mode(n, f.reg, ""));
    EXPECT_FALSE(accepts_mode(n, f.reg, "tight"));
}

TEST(Modes, DeclaredModesReplaceEnable)
{
    Fixture f;
    Node const n = f.add("drive", "m");

    EXPECT_EQ(modes_of(n, f.reg),
              (std::vector<std::string>{ "position", "speed", "torque", "disable" }));
    EXPECT_TRUE(accepts_mode(n, f.reg, "torque"));
    EXPECT_TRUE(accepts_mode(n, f.reg, "disable"));
    EXPECT_FALSE(accepts_mode(n, f.reg, "enable"));
}

TEST(Modes, TypesDeclaringDisableEmptyOrDuplicateModesAreRefused)
{
    NodeRegistry reg;
    NodeType nt = piper_modes_test::make_drive();
    for (auto const& modes : { std::vector<std::string>{ "run", "disable" },
                               std::vector<std::string>{ "run", "" },
                               std::vector<std::string>{ "run", "run" } })
    {
        nt.modes = modes;
        EXPECT_FALSE(reg.add(nt));
    }
    nt.modes = { "run" };
    EXPECT_TRUE(reg.add(nt));

    auto const loaded = v2::deserialize_registry(R"({
        "version": 3,
        "types": [ { "type": "bad", "modes": ["run", "disable"] } ]
    })");
    EXPECT_EQ(loaded.diagnostics.size(), 1u);
    EXPECT_EQ(loaded.registry.find("bad"), nullptr);
}

TEST(Modes, NewNodeStartsWithItsTypeModesOrEnable)
{
    Fixture f;
    EXPECT_EQ(f.add("drive", "d").modes, (std::vector<std::string>{ "position", "speed", "torque" }));
    EXPECT_EQ(f.add("constant<float>", "k").modes, (std::vector<std::string>{ "enable" }));
}

TEST(Modes, NodeModesAreAddedToTypeModes)
{
    Fixture f;
    Node const n = f.add("drive", "d");
    ASSERT_FALSE(f.g.add_node_mode(n.id, "homing", std::size_t(-1)));
    Node const& live = *f.g.find_node(n.id);

    EXPECT_EQ(modes_of(live, f.reg),
              (std::vector<std::string>{ "position", "speed", "torque", "homing", "disable" }));
    EXPECT_TRUE(accepts_mode(live, f.reg, "homing"));
    EXPECT_FALSE(accepts_mode(live, f.reg, "enable"));
}

TEST(Modes, NodeModeEditsKeepTheListValid)
{
    Fixture f;
    Node const k = f.add("constant<float>", "k");

    EXPECT_EQ(f.g.add_node_mode(k.id, "disable", 0), GraphError::InvalidModeName);
    EXPECT_EQ(f.g.add_node_mode(k.id, "", 0), GraphError::InvalidModeName);
    EXPECT_EQ(f.g.add_node_mode(k.id, "enable", 0), GraphError::DuplicateMode);
    EXPECT_EQ(f.g.remove_node_mode(k.id, "enable"), GraphError::LastMode);

    ASSERT_FALSE(f.g.rename_node_mode(k.id, "enable", "tight"));
    ASSERT_FALSE(f.g.add_node_mode(k.id, "loose", std::size_t(-1)));
    EXPECT_EQ(f.g.rename_node_mode(k.id, "tight", "loose"), GraphError::DuplicateMode);
    EXPECT_EQ(f.g.find_node(k.id)->modes, (std::vector<std::string>{ "tight", "loose" }));
}

TEST(Modes, RenamingAModeRenamesOverloadsAndProfileEntries)
{
    Fixture f;
    Node const k = f.add("constant<float>", "k");
    ASSERT_FALSE(f.g.set_mode_overload(k.id, "value", "enable", "5"));
    Profile p;
    p.name           = "run";
    p.per_node[k.id] = "enable";
    f.g.add_profile(p);

    ASSERT_FALSE(f.g.rename_node_mode(k.id, "enable", "tight"));
    Node const& live = *f.g.find_node(k.id);
    EXPECT_EQ(live.find_attr("value")->mode_overloads.count("enable"), 0u);
    EXPECT_EQ(live.find_attr("value")->mode_overloads.at("tight"), "5");
    EXPECT_EQ(f.g.profiles().back().per_node.at(k.id), "tight");

    ASSERT_FALSE(f.g.add_node_mode(k.id, "loose", std::size_t(-1)));
    ASSERT_FALSE(f.g.set_mode_overload(k.id, "value", "loose", "1"));
    ASSERT_FALSE(f.g.remove_node_mode(k.id, "loose"));
    EXPECT_EQ(f.g.find_node(k.id)->find_attr("value")->mode_overloads.count("loose"), 0u);
}

TEST(Modes, UnknownTypeAcceptsAnyMode)
{
    Fixture f;
    Node const n = f.add("drive", "m");
    NodeRegistry empty;

    EXPECT_EQ(modes_of(n, empty),
              (std::vector<std::string>{ "position", "speed", "torque", "disable" }));
    EXPECT_TRUE(accepts_mode(n, empty, "anything"));
}

TEST(Modes, CheckProfileModesFlagsUndeclaredModes)
{
    Fixture f;
    Node const motor = f.add("drive", "m");
    Node const k     = f.add("constant<float>", "k");

    Profile p;
    p.name               = "run";
    p.per_node[motor.id] = "enable";
    p.per_node[k.id]     = "disable";
    f.g.add_profile(p);

    auto const diags = check_profile_modes(f.g, f.reg);
    ASSERT_EQ(diags.size(), 1u);
    EXPECT_EQ(diags[0].kind,    Diagnostic::Kind::UnknownMode);
    EXPECT_EQ(diags[0].node_id, motor.id);

    f.g.set_node_mode("run", motor.id, "torque");
    EXPECT_TRUE(check_profile_modes(f.g, f.reg).empty());
}

TEST(Modes, CheckProfileModesFlagsMissingModes)
{
    Fixture f;
    Node const k = f.add("constant<float>", "k");

    Profile p;
    p.name = "run";
    f.g.add_profile(p);

    auto const diags = check_profile_modes(f.g, f.reg);
    ASSERT_EQ(diags.size(), 1u);
    EXPECT_EQ(diags[0].kind,    Diagnostic::Kind::MissingMode);
    EXPECT_EQ(diags[0].node_id, k.id);

    Profile with_empty;
    with_empty.name           = "safe";
    with_empty.per_node[k.id] = "";
    f.g.add_profile(with_empty);
    auto const both = check_profile_modes(f.g, f.reg);
    ASSERT_EQ(both.size(), 2u);
    EXPECT_EQ(both[1].kind, Diagnostic::Kind::MissingMode);
}

TEST(Modes, FillMissingModesUsesFirstModeAndKeepsExistingEntries)
{
    Fixture f;
    Node const drive = f.add("drive", "d");
    Node const k     = f.add("constant<float>", "k");
    ASSERT_FALSE(f.g.rename_node_mode(k.id, "enable", "tight"));

    Profile p;
    p.name               = "run";
    p.per_node[drive.id] = "torque";
    fill_missing_modes(p, f.g, f.reg);

    EXPECT_EQ(p.per_node.at(drive.id), "torque");
    EXPECT_EQ(p.per_node.at(k.id),     "tight");
}

TEST(Modes, NodeModesAndOverloadsRoundTrip)
{
    Fixture f;
    Node const k = f.add("constant<float>", "k");
    Node const e = f.add("constant<float>", "plain");
    ASSERT_FALSE(f.g.rename_node_mode(k.id, "enable", "tight"));
    ASSERT_FALSE(f.g.add_node_mode(k.id, "loose", std::size_t(-1)));
    ASSERT_TRUE(f.g.set_attr_value(k.id, "value", "10"));
    ASSERT_FALSE(f.g.set_mode_overload(k.id, "value", "tight", "100"));

    std::string const text = v2::serialize(f.g);
    EXPECT_NE(text.find("\"mode_overloads\""), std::string::npos);
    EXPECT_NE(text.find("\"tight\": 100"), std::string::npos);

    auto const loaded = v2::deserialize(text, f.reg);
    EXPECT_TRUE(loaded.diagnostics.empty());
    Node const* back = loaded.graph.find_node(k.id);
    ASSERT_NE(back, nullptr);
    EXPECT_EQ(back->modes, (std::vector<std::string>{ "tight", "loose" }));
    EXPECT_EQ(back->find_attr("value")->value, "10");
    EXPECT_EQ(back->find_attr("value")->mode_overloads.at("tight"), "100");
    EXPECT_EQ(loaded.graph.find_node(e.id)->modes, (std::vector<std::string>{ "enable" }));
}

TEST(Modes, LoaderAddsMissingTypeModesToNodes)
{
    Fixture f;
    auto const loaded = v2::deserialize(R"({
        "version": 3,
        "nodes": [ { "id": 1, "type": "drive", "name": "d", "modes": ["homing"] } ]
    })", f.reg);
    Node const* d = loaded.graph.find_node(1);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->modes, (std::vector<std::string>{ "position", "speed", "torque", "homing" }));
}

TEST(Modes, LoaderDropsDisableAndDuplicatesFromNodeModes)
{
    Fixture f;
    auto const loaded = v2::deserialize(R"({
        "version": 3,
        "nodes": [ { "id": 1, "type": "constant<float>", "name": "k",
                     "modes": ["tight", "disable", "tight", ""] } ]
    })", f.reg);
    int schema_errors = 0;
    for (auto const& d : loaded.diagnostics)
    {
        if (d.kind == Diagnostic::Kind::SchemaError and d.node_id == 1u)
        {
            ++schema_errors;
        }
    }
    EXPECT_EQ(schema_errors, 1);
    EXPECT_EQ(loaded.graph.find_node(1)->modes, (std::vector<std::string>{ "tight" }));
}

TEST(Modes, OmittedEnableSurvivesATypeThatGainedModes)
{
    Fixture f;
    auto const loaded = v2::deserialize(R"({
        "version": 3,
        "nodes": [ { "id": 1, "type": "drive", "name": "d" } ]
    })", f.reg);
    EXPECT_EQ(loaded.graph.find_node(1)->modes,
              (std::vector<std::string>{ "position", "speed", "torque", "enable" }));
}

TEST(Modes, EditsAcceptANameBorrowedFromTheModeList)
{
    Fixture f;
    Node const k = f.add("constant<float>", "k");
    ASSERT_FALSE(f.g.rename_node_mode(k.id, "enable", "tight"));
    ASSERT_FALSE(f.g.add_node_mode(k.id, "loose", std::size_t(-1)));
    ASSERT_FALSE(f.g.set_mode_overload(k.id, "value", "tight", "1"));
    ASSERT_FALSE(f.g.set_mode_overload(k.id, "value", "loose", "2"));

    ASSERT_FALSE(f.g.remove_node_mode(k.id, f.g.find_node(k.id)->modes[0]));
    Node const& live = *f.g.find_node(k.id);
    EXPECT_EQ(live.modes, (std::vector<std::string>{ "loose" }));
    EXPECT_EQ(live.find_attr("value")->mode_overloads.count("tight"), 0u);
    EXPECT_EQ(live.find_attr("value")->mode_overloads.at("loose"), "2");

    ASSERT_FALSE(f.g.rename_node_mode(k.id, live.modes[0], "fast"));
    EXPECT_EQ(f.g.find_node(k.id)->find_attr("value")->mode_overloads.at("fast"), "2");
}

TEST(Modes, ModeEditErrorsExplainTheFailure)
{
    Fixture f;
    Node const k = f.add("constant<float>", "k");
    EXPECT_EQ(f.g.add_node_mode(999, "x", 0), GraphError::UnknownNode);
    EXPECT_EQ(f.g.remove_node_mode(k.id, "nope"), GraphError::UnknownMode);
    EXPECT_EQ(f.g.set_mode_overload(k.id, "nope", "enable", "1"), GraphError::UnknownAttribute);
    EXPECT_EQ(f.g.clear_mode_overload(k.id, "value", "enable"), GraphError::UnknownMode);
    EXPECT_EQ(f.g.rename_node_mode(k.id, "enable", "disable"), GraphError::InvalidModeName);
    EXPECT_EQ(make_error_code(GraphError::LastMode).message(), "a node keeps at least one mode");
}
