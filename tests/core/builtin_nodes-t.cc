#include <gtest/gtest.h>

#include "piper/builtin_nodes.h"
#include "piper/registry.h"

using namespace piper;

TEST(BuiltinNodes, RegistersExpectedTypes)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    EXPECT_NE(reg.find("constant<float>"), nullptr);
    EXPECT_NE(reg.find("constant<int>"),   nullptr);
    EXPECT_NE(reg.find("sin_wave"),        nullptr);
    EXPECT_NE(reg.find("random"),          nullptr);
    EXPECT_NE(reg.find("add"),             nullptr);
    EXPECT_NE(reg.find("low_pass"),        nullptr);
    EXPECT_NE(reg.find("cast<int>"),       nullptr);
    EXPECT_NE(reg.find("cast<float>"),     nullptr);
    EXPECT_NE(reg.find("probe<float>"),    nullptr);
    EXPECT_NE(reg.find("probe<int>"),      nullptr);
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

    auto const* ci = reg.find("constant<int>");
    ASSERT_NE(ci, nullptr);
    ASSERT_EQ(ci->attributes.size(), 2u);
    EXPECT_EQ(ci->attributes[0].data_type, "int");
    EXPECT_EQ(ci->attributes[1].data_type, "int");
}

TEST(BuiltinNodes, AddTypeHasExpectedAttrs)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    auto const* add = reg.find("add");
    ASSERT_NE(add, nullptr);
    ASSERT_EQ(add->attributes.size(), 3u);
    EXPECT_EQ(add->attributes[0].name, "a");
    EXPECT_EQ(add->attributes[0].role, AttributeSpec::Role::Input);
    EXPECT_EQ(add->attributes[2].name, "out");
    EXPECT_EQ(add->attributes[2].role, AttributeSpec::Role::Output);
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
    auto const* nt = reg.find("sin_wave");
    ASSERT_NE(nt, nullptr);
    ASSERT_EQ(nt->attributes.size(), 4u);
    EXPECT_EQ(nt->attributes[0].name, "frequency");
    EXPECT_EQ(nt->attributes[0].role, AttributeSpec::Role::Member);
    EXPECT_EQ(nt->attributes[3].name, "out");
    EXPECT_EQ(nt->attributes[3].role, AttributeSpec::Role::Output);
}

TEST(BuiltinNodes, LowPassShape)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);
    auto const* nt = reg.find("low_pass");
    ASSERT_NE(nt, nullptr);
    ASSERT_EQ(nt->attributes.size(), 3u);
    EXPECT_EQ(nt->attributes[0].role, AttributeSpec::Role::Input);
    EXPECT_EQ(nt->attributes[1].role, AttributeSpec::Role::Member);
    EXPECT_EQ(nt->attributes[2].role, AttributeSpec::Role::Output);
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

    auto const* probe_i = reg.find("probe<int>");
    ASSERT_NE(probe_i, nullptr);
    EXPECT_EQ(probe_i->attributes[0].data_type, "int");
}

TEST(BuiltinNodes, CastsHaveOpposingTypes)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    auto const* to_int = reg.find("cast<int>");
    ASSERT_NE(to_int, nullptr);
    EXPECT_EQ(to_int->attributes[0].data_type, "float");
    EXPECT_EQ(to_int->attributes[1].data_type, "int");

    auto const* to_float = reg.find("cast<float>");
    ASSERT_NE(to_float, nullptr);
    EXPECT_EQ(to_float->attributes[0].data_type, "int");
    EXPECT_EQ(to_float->attributes[1].data_type, "float");
}
