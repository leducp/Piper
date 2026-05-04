#include <gtest/gtest.h>

#include "piper/registry.h"
#include "piper/serialize_v2.h"

using namespace piper;

NodeType make_bus_type()
{
    NodeType nt;
    nt.type     = "Bus";
    nt.help     = "I/O hub; direction resolves per stage";
    nt.category = "io";
    nt.attributes = {
        { "torque_cmd",  "vec3",  AttributeSpec::Role::Output, ""    },
        { "torque_meas", "vec3",  AttributeSpec::Role::Input,  ""    },
        { "gain",        "float", AttributeSpec::Role::Member, "1.0" },
    };
    return nt;
}

NodeType make_filter_type()
{
    NodeType nt;
    nt.type = "LowPass";
    nt.attributes = {
        { "in",     "vec3",  AttributeSpec::Role::Input,  ""    },
        { "out",    "vec3",  AttributeSpec::Role::Output, ""    },
        { "cutoff", "float", AttributeSpec::Role::Member, "10.0" },
    };
    return nt;
}

NodeRegistry default_registry()
{
    NodeRegistry r;
    r.add(make_bus_type());
    r.add(make_filter_type());
    return r;
}

Graph build_motor_graph()
{
    Graph g;
    auto bus    = make_bus_type();
    auto filter = make_filter_type();

    auto bus_id = g.add_node(bus,    "main_bus", "control",  { 100.0f, 100.0f });
    auto flt_id = g.add_node(filter, "lowpass",  "feedback", { 250.0f, 100.0f });

    g.set_attr_stages(bus_id, "torque_cmd",  { "control"  });
    g.set_attr_stages(bus_id, "torque_meas", { "feedback" });
    g.set_attr_value(bus_id, "gain", "0.5");

    g.add_link({ bus_id, "torque_cmd"  }, { flt_id, "in" }, "vec3");
    g.add_link({ flt_id, "out"         }, { bus_id, "torque_meas" }, "vec3");

    g.add_stage({ "control",  rgba::from_components(0xFF, 0x00, 0x00, 0xFF) });
    g.add_stage({ "feedback", rgba::from_components(0x00, 0xFF, 0x00, 0xFF) });

    ModeProfile p;
    p.name        = "default";
    p.per_node[bus_id] = "enable";
    p.per_node[flt_id] = "enable";
    g.add_mode_profile(p);

    ModeProfile safety;
    safety.name             = "safety_mode";
    safety.per_node[bus_id] = "disable";
    safety.per_node[flt_id] = "enable";
    g.add_mode_profile(safety);

    g.set_default_mode_name("default");

    return g;
}

TEST(SerializeV2, RoundTripPreservesNodes)
{
    Graph const original = build_motor_graph();
    auto registry = default_registry();

    std::string text = v2::serialize(original);
    auto loaded = v2::deserialize(text, registry);

    EXPECT_TRUE(loaded.diagnostics.empty()) << "unexpected diagnostics on clean round-trip";
    ASSERT_EQ(loaded.graph.nodes().size(), original.nodes().size());

    for (std::size_t i = 0; i < original.nodes().size(); ++i)
    {
        auto const& a = original.nodes()[i];
        auto const& b = loaded.graph.nodes()[i];
        EXPECT_EQ(a.id, b.id);
        EXPECT_EQ(a.type, b.type);
        EXPECT_EQ(a.name, b.name);
        EXPECT_EQ(a.stage, b.stage);
        EXPECT_EQ(a.pos, b.pos);
        ASSERT_EQ(a.attrs.size(), b.attrs.size());
        for (std::size_t k = 0; k < a.attrs.size(); ++k)
        {
            EXPECT_EQ(a.attrs[k].name,      b.attrs[k].name);
            EXPECT_EQ(a.attrs[k].data_type, b.attrs[k].data_type);
            EXPECT_EQ(a.attrs[k].role,      b.attrs[k].role);
            EXPECT_EQ(a.attrs[k].value,     b.attrs[k].value);
            EXPECT_EQ(a.attrs[k].stages,    b.attrs[k].stages);
        }
    }
}

TEST(SerializeV2, RoundTripPreservesLinks)
{
    Graph const original = build_motor_graph();
    auto registry = default_registry();

    auto loaded = v2::deserialize(v2::serialize(original), registry);

    ASSERT_EQ(loaded.graph.links().size(), original.links().size());
    for (std::size_t i = 0; i < original.links().size(); ++i)
    {
        auto const& a = original.links()[i];
        auto const& b = loaded.graph.links()[i];
        EXPECT_EQ(a.id, b.id);
        EXPECT_EQ(a.from, b.from);
        EXPECT_EQ(a.to,   b.to);
        EXPECT_EQ(a.data_type, b.data_type);
    }
}

TEST(SerializeV2, RoundTripPreservesStages)
{
    Graph const original = build_motor_graph();
    auto registry = default_registry();

    auto loaded = v2::deserialize(v2::serialize(original), registry);

    ASSERT_EQ(loaded.graph.stages().size(), original.stages().size());
    for (std::size_t i = 0; i < original.stages().size(); ++i)
    {
        EXPECT_EQ(original.stages()[i].name,  loaded.graph.stages()[i].name);
        EXPECT_EQ(original.stages()[i].color, loaded.graph.stages()[i].color);
    }
}

TEST(SerializeV2, RoundTripPreservesModeProfiles)
{
    Graph const original = build_motor_graph();
    auto registry = default_registry();

    auto loaded = v2::deserialize(v2::serialize(original), registry);

    ASSERT_EQ(loaded.graph.mode_profiles().size(), original.mode_profiles().size());
    for (std::size_t i = 0; i < original.mode_profiles().size(); ++i)
    {
        auto const& a = original.mode_profiles()[i];
        auto const& b = loaded.graph.mode_profiles()[i];
        EXPECT_EQ(a.name,     b.name);
        EXPECT_EQ(a.per_node, b.per_node);
    }
    EXPECT_EQ(loaded.graph.default_mode_name(), original.default_mode_name());
}

TEST(SerializeV2, ReserveIdsAboveAfterLoad)
{
    Graph const original = build_motor_graph();
    auto registry = default_registry();

    auto loaded = v2::deserialize(v2::serialize(original), registry);

    ASSERT_FALSE(loaded.graph.nodes().empty());
    ASSERT_FALSE(loaded.graph.links().empty());
    NodeId const max_loaded_node = loaded.graph.nodes().back().id;
    LinkId const max_loaded_link = loaded.graph.links().back().id;

    auto bus  = make_bus_type();
    auto fresh = loaded.graph.add_node(bus, "fresh", "", {});
    EXPECT_GT(fresh, max_loaded_node);

    Node const* fresh_node = loaded.graph.find_node(fresh);
    ASSERT_NE(fresh_node, nullptr);
    auto const& bus_node = loaded.graph.nodes()[0];
    auto fresh_link = loaded.graph.add_link(
        { bus_node.id, "torque_cmd" }, { fresh, "torque_meas" }, "vec3");
    EXPECT_GT(fresh_link, max_loaded_link);
}

TEST(SerializeV2, RoundTripIsDeterministic)
{
    Graph const original = build_motor_graph();
    auto registry = default_registry();

    std::string const text_a = v2::serialize(original);
    auto loaded = v2::deserialize(text_a, registry);
    ASSERT_TRUE(loaded.diagnostics.empty());
    std::string const text_b = v2::serialize(loaded.graph);
    EXPECT_EQ(text_a, text_b);
}

TEST(SerializeV2, RoundTripPreservesNodeNote)
{
    auto registry = default_registry();
    Graph g;
    auto bus    = make_bus_type();
    auto bus_id = g.add_node(bus, "main", "", Point{});
    g.set_node_note(bus_id, "tuned for joint X\nhand-edited 2026");

    auto loaded = v2::deserialize(v2::serialize(g), registry);
    ASSERT_EQ(loaded.graph.nodes().size(), 1u);
    EXPECT_EQ(loaded.graph.nodes().front().note,
              "tuned for joint X\nhand-edited 2026");
}

TEST(SerializeV2, RoundTripPreservesAnnotations)
{
    auto registry = default_registry();
    Graph g;

    Annotation a;
    a.pos   = Point{ 100.0f, 200.0f };
    a.size  = Point{ 300.0f, 150.0f };
    a.text  = "first frame";
    a.color = rgba::from_components(0xFF, 0xC0, 0x40, 0x80);
    auto a_id = g.add_annotation(a);

    Annotation b;
    b.pos   = Point{ 500.0f, 600.0f };
    b.size  = Point{ 200.0f, 100.0f };
    b.text  = "";
    b.color = rgba::from_components(0x00, 0x80, 0xFF, 0xC0);
    auto b_id = g.add_annotation(b);

    auto loaded = v2::deserialize(v2::serialize(g), registry);
    ASSERT_EQ(loaded.graph.annotations().size(), 2u);

    Annotation const* ra = loaded.graph.find_annotation(a_id);
    Annotation const* rb = loaded.graph.find_annotation(b_id);
    ASSERT_NE(ra, nullptr);
    ASSERT_NE(rb, nullptr);
    EXPECT_EQ(ra->pos,   a.pos);
    EXPECT_EQ(ra->size,  a.size);
    EXPECT_EQ(ra->text,  a.text);
    EXPECT_EQ(ra->color, a.color);
    EXPECT_EQ(rb->pos,   b.pos);
    EXPECT_EQ(rb->size,  b.size);
    EXPECT_EQ(rb->text,  b.text);
    EXPECT_EQ(rb->color, b.color);
}

TEST(SerializeV2, AnnotationIdReservedAfterLoad)
{
    auto registry = default_registry();
    Graph g;
    Annotation a;
    a.text = "anno";
    auto loaded_id = g.add_annotation(a);

    auto loaded = v2::deserialize(v2::serialize(g), registry);

    Annotation b;
    auto fresh_id = loaded.graph.add_annotation(b);
    EXPECT_GT(fresh_id, loaded_id);
}

TEST(SerializeV2, RoundTripPreservesLabels)
{
    auto registry = default_registry();
    Graph g;

    auto a_id = g.add_label(LabelKind::In,  "tap",     Point{ 100.0f, 200.0f });
    auto b_id = g.add_label(LabelKind::Out, "tap",     Point{ 500.0f, 200.0f });
    auto c_id = g.add_label(LabelKind::In,  "feedback", Point{   0.0f,   0.0f });
    (void)c_id;

    auto loaded = v2::deserialize(v2::serialize(g), registry);
    ASSERT_EQ(loaded.graph.labels().size(), 3u);

    Label const* la = loaded.graph.find_label(a_id);
    Label const* lb = loaded.graph.find_label(b_id);
    ASSERT_NE(la, nullptr);
    ASSERT_NE(lb, nullptr);
    EXPECT_EQ(la->kind, LabelKind::In);
    EXPECT_EQ(la->name, "tap");
    Point const a_pos{ 100.0f, 200.0f };
    EXPECT_EQ(la->pos, a_pos);
    EXPECT_EQ(lb->kind, LabelKind::Out);
    EXPECT_EQ(lb->name, "tap");
}

TEST(SerializeV2, MigratesOldLabelNodes)
{
    auto registry = default_registry();
    // V1-ish: labels were Node entries with type "label_in"/"label_out"
    // and a "name" Member attribute.
    std::string const json = R"({
        "version": 2,
        "pipelines": [{
            "name": "main",
            "nodes": [
                { "id": 7, "type": "label_in",  "name": "Aold", "stage": "",
                  "pos": [10, 20],
                  "attrs": [{ "name": "name", "data_type": "string",
                              "role": "member", "value": "tap" }] },
                { "id": 8, "type": "label_out", "name": "Bold", "stage": "",
                  "pos": [30, 40],
                  "attrs": [{ "name": "name", "data_type": "string",
                              "role": "member", "value": "tap" }] }
            ],
            "links": [],
            "stages": [],
            "modes":  []
        }]
    })";

    auto bundle = v2::deserialize_bundle(json, registry);
    ASSERT_EQ(bundle.pipelines.size(), 1u);
    auto const& g = bundle.pipelines.front().graph;
    EXPECT_TRUE(g.nodes().empty());
    ASSERT_EQ(g.labels().size(), 2u);

    Label const* a = g.find_label(7);
    Label const* b = g.find_label(8);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(a->kind, LabelKind::In);
    EXPECT_EQ(a->name, "tap");
    EXPECT_EQ(b->kind, LabelKind::Out);
    EXPECT_EQ(b->name, "tap");
}

TEST(SerializeV2, EmptyGraphRoundTrips)
{
    Graph empty_graph;
    auto registry = default_registry();

    std::string text = v2::serialize(empty_graph);
    auto loaded = v2::deserialize(text, registry);

    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_TRUE(loaded.graph.nodes().empty());
    EXPECT_TRUE(loaded.graph.links().empty());
    EXPECT_TRUE(loaded.graph.stages().empty());
    EXPECT_TRUE(loaded.graph.mode_profiles().empty());
}

TEST(SerializeV2, ThrowsOnMalformedJson)
{
    NodeRegistry r;
    EXPECT_THROW(v2::deserialize("{ this is not json", r), std::runtime_error);
    EXPECT_THROW(v2::deserialize("{}", r), std::runtime_error);  // version 0
}

TEST(SerializeV2, ThrowsOnUnsupportedVersion)
{
    NodeRegistry r;
    EXPECT_THROW(v2::deserialize(R"({"version": 1})", r), std::runtime_error);
    EXPECT_THROW(v2::deserialize(R"({"version": 99})", r), std::runtime_error);
}
