#include <gtest/gtest.h>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/registry.h"

using namespace piper;

TEST(BuiltinNodes, RegistersExpectedTypes)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    EXPECT_NE(reg.find("constant<float>"), nullptr);
    EXPECT_NE(reg.find("constant<int32_t>"),   nullptr);
    EXPECT_NE(reg.find("sin_wave<float>"),        nullptr);
    EXPECT_NE(reg.find("random"),          nullptr);
    EXPECT_NE(reg.find("add<float>"),      nullptr);
    EXPECT_NE(reg.find("add<double>"),     nullptr);
    EXPECT_NE(reg.find("add<int32_t>"),    nullptr);
    EXPECT_NE(reg.find("multiply<float>"),   nullptr);
    EXPECT_NE(reg.find("multiply<double>"),  nullptr);
    EXPECT_NE(reg.find("multiply<int32_t>"), nullptr);
    EXPECT_NE(reg.find("abs<float>"),      nullptr);
    EXPECT_NE(reg.find("abs<double>"),     nullptr);
    EXPECT_NE(reg.find("abs<int32_t>"),    nullptr);
    EXPECT_NE(reg.find("subtract<float>"),   nullptr);
    EXPECT_NE(reg.find("subtract<double>"),  nullptr);
    EXPECT_NE(reg.find("subtract<int32_t>"), nullptr);
    EXPECT_NE(reg.find("mux3<float>"),       nullptr);
    EXPECT_NE(reg.find("mux3<double>"),      nullptr);
    EXPECT_NE(reg.find("mux3<int32_t>"),     nullptr);
    EXPECT_NE(reg.find("pid<float>"),        nullptr);
    EXPECT_NE(reg.find("pid<double>"),       nullptr);
    EXPECT_NE(reg.find("preset3<float>"),    nullptr);
    EXPECT_NE(reg.find("preset3<double>"),   nullptr);
    EXPECT_NE(reg.find("preset3<int32_t>"),  nullptr);
    EXPECT_NE(reg.find("clamp<float>"),      nullptr);
    EXPECT_NE(reg.find("clamp<double>"),     nullptr);
    EXPECT_NE(reg.find("clamp<int32_t>"),    nullptr);
    EXPECT_NE(reg.find("constant<vec2<float>>"), nullptr);
    EXPECT_NE(reg.find("constant<vec3<float>>"), nullptr);
    EXPECT_NE(reg.find("add<vec2<float>>"),      nullptr);
    EXPECT_NE(reg.find("add<vec3<float>>"),      nullptr);
    EXPECT_NE(reg.find("subtract<vec2<float>>"), nullptr);
    EXPECT_NE(reg.find("subtract<vec3<float>>"), nullptr);
    EXPECT_NE(reg.find("low_pass<float>"),        nullptr);
    EXPECT_NE(reg.find("cast<float,int32_t>"), nullptr);
    EXPECT_NE(reg.find("cast<int32_t,float>"), nullptr);
    EXPECT_NE(reg.find("probe<float>"),    nullptr);
    EXPECT_NE(reg.find("probe<int32_t>"),      nullptr);
    EXPECT_NE(reg.find("jacobian_2x2"),    nullptr);
    EXPECT_NE(reg.find("motor"),           nullptr);

    EXPECT_GE(reg.size(), 12u);
}

TEST(BuiltinNodes, ConstantsExposeMemberValueAndTypedOutput)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    auto const* cf = reg.find("constant<float>");
    ASSERT_NE(cf, nullptr);
    ASSERT_EQ(cf->attributes.size(), 2u);
    EXPECT_EQ(cf->attributes[0].name,      "value");
    EXPECT_EQ(cf->attributes[0].role,      AttributeSpec::Role::Member);
    EXPECT_EQ(cf->attributes[0].data_type, "float");
    EXPECT_EQ(cf->attributes[1].name,      "out");
    EXPECT_EQ(cf->attributes[1].role,      AttributeSpec::Role::Output);
    EXPECT_EQ(cf->attributes[1].data_type, "float");

    auto const* ci = reg.find("constant<int32_t>");
    ASSERT_NE(ci, nullptr);
    ASSERT_EQ(ci->attributes.size(), 2u);
    EXPECT_EQ(ci->attributes[0].data_type, "int32_t");
    EXPECT_EQ(ci->attributes[1].data_type, "int32_t");
}

TEST(BuiltinNodes, AddTypeHasExpectedAttrs)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    auto const* add_f = reg.find("add<float>");
    ASSERT_NE(add_f, nullptr);
    ASSERT_EQ(add_f->attributes.size(), 3u);
    EXPECT_EQ(add_f->attributes[0].name,      "a");
    EXPECT_EQ(add_f->attributes[0].role,      AttributeSpec::Role::Input);
    EXPECT_EQ(add_f->attributes[0].data_type, "float");
    EXPECT_EQ(add_f->attributes[2].name,      "out");
    EXPECT_EQ(add_f->attributes[2].role,      AttributeSpec::Role::Output);
    EXPECT_EQ(add_f->attributes[2].data_type, "float");

    auto const* add_i = reg.find("add<int32_t>");
    ASSERT_NE(add_i, nullptr);
    EXPECT_EQ(add_i->attributes[0].data_type, "int32_t");
    EXPECT_EQ(add_i->attributes[2].data_type, "int32_t");
}

TEST(BuiltinNodes, RegistrationIsIdempotent)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);
    auto const initial_size = reg.size();
    register_builtin_nodes(reg);   // already registered: each add returns false
    EXPECT_EQ(reg.size(), initial_size);
}

TEST(BuiltinNodes, SinWaveShape)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);
    auto const* nt = reg.find("sin_wave<float>");
    ASSERT_NE(nt, nullptr);
    ASSERT_EQ(nt->attributes.size(), 6u);
    EXPECT_EQ(nt->attributes[0].name, "frequency");
    EXPECT_EQ(nt->attributes[0].role, AttributeSpec::Role::Member);
    EXPECT_EQ(nt->attributes[3].name, "dt");
    EXPECT_EQ(nt->attributes[3].role, AttributeSpec::Role::Member);
    EXPECT_EQ(nt->attributes[4].name, "dt_in");
    EXPECT_EQ(nt->attributes[4].role, AttributeSpec::Role::Input);
    EXPECT_EQ(nt->attributes[5].name, "out");
    EXPECT_EQ(nt->attributes[5].role, AttributeSpec::Role::Output);
}

TEST(BuiltinNodes, LowPassShape)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);
    auto const* nt = reg.find("low_pass<float>");
    ASSERT_NE(nt, nullptr);
    ASSERT_EQ(nt->attributes.size(), 5u);
    EXPECT_EQ(nt->attributes[0].role, AttributeSpec::Role::Input);   // in
    EXPECT_EQ(nt->attributes[1].role, AttributeSpec::Role::Member);  // cutoff
    EXPECT_EQ(nt->attributes[2].name, "dt");
    EXPECT_EQ(nt->attributes[2].role, AttributeSpec::Role::Member);
    EXPECT_EQ(nt->attributes[3].name, "dt_in");
    EXPECT_EQ(nt->attributes[3].role, AttributeSpec::Role::Input);
    EXPECT_EQ(nt->attributes[4].role, AttributeSpec::Role::Output);
}

TEST(BuiltinNodes, ProbeIsSinkOnly)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);
    auto const* probe_f = reg.find("probe<float>");
    ASSERT_NE(probe_f, nullptr);
    ASSERT_EQ(probe_f->attributes.size(), 1u);
    EXPECT_EQ(probe_f->attributes[0].role, AttributeSpec::Role::Input);
    EXPECT_EQ(probe_f->attributes[0].data_type, "float");

    auto const* probe_i = reg.find("probe<int32_t>");
    ASSERT_NE(probe_i, nullptr);
    EXPECT_EQ(probe_i->attributes[0].data_type, "int32_t");
}

TEST(BuiltinNodes, Preset3LabelMembersAreFlaggedForModePicker)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    auto const* nt = reg.find("preset3<float>");
    ASSERT_NE(nt, nullptr);

    int label_count = 0;
    for (auto const& spec : nt->attributes)
    {
        if (spec.is_mode_label)
        {
            EXPECT_EQ(spec.data_type, "string");
            EXPECT_EQ(spec.role,      AttributeSpec::Role::Member);
            ++label_count;
        }
    }
    EXPECT_EQ(label_count, 3);
}

TEST(BuiltinNodes, ModeLabelsAdvertisedByReadsLiveAttrValues)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    auto const* nt = reg.find("preset3<float>");
    ASSERT_NE(nt, nullptr);

    Graph g;
    g.add_stage(Stage{ "ctl", 0xFFFFFFFFu });
    auto const id = g.add_node(*nt, "gain", "ctl", Point{ 0.0f, 0.0f });

    g.set_attr_value(id, "label0", "tight");
    g.set_attr_value(id, "label1", "loose");
    g.set_attr_value(id, "label2", "bypass");

    Node const* node = g.find_node(id);
    ASSERT_NE(node, nullptr);

    auto const labels = mode_labels_advertised_by(*node, reg);
    ASSERT_EQ(labels.size(), 3u);
    EXPECT_EQ(labels[0], "tight");
    EXPECT_EQ(labels[1], "loose");
    EXPECT_EQ(labels[2], "bypass");
}

TEST(BuiltinNodes, ModeLabelsAdvertisedByEmptyForUnflaggedTypes)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    auto const* nt = reg.find("constant<float>");
    ASSERT_NE(nt, nullptr);

    Graph g;
    g.add_stage(Stage{ "ctl", 0xFFFFFFFFu });
    auto const id = g.add_node(*nt, "k", "ctl", Point{ 0.0f, 0.0f });

    Node const* node = g.find_node(id);
    ASSERT_NE(node, nullptr);

    EXPECT_TRUE(mode_labels_advertised_by(*node, reg).empty());
}

TEST(BuiltinNodes, CastsHaveOpposingTypes)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    auto const* to_int = reg.find("cast<float,int32_t>");
    ASSERT_NE(to_int, nullptr);
    EXPECT_EQ(to_int->attributes[0].data_type, "float");
    EXPECT_EQ(to_int->attributes[1].data_type, "int32_t");

    auto const* to_float = reg.find("cast<int32_t,float>");
    ASSERT_NE(to_float, nullptr);
    EXPECT_EQ(to_float->attributes[0].data_type, "int32_t");
    EXPECT_EQ(to_float->attributes[1].data_type, "float");

    auto const* widen = reg.find("cast<uint32_t,double>");
    ASSERT_NE(widen, nullptr);
    EXPECT_EQ(widen->attributes[0].data_type, "uint32_t");
    EXPECT_EQ(widen->attributes[1].data_type, "double");
}
