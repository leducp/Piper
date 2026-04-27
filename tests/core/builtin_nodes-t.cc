#include <gtest/gtest.h>

#include "piper/builtin_nodes.h"
#include "piper/registry.h"

using namespace piper;

TEST(BuiltinNodes, RegistersExpectedTypes)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    EXPECT_NE(reg.find("SinWave"),      nullptr);
    EXPECT_NE(reg.find("Random"),       nullptr);
    EXPECT_NE(reg.find("Add"),          nullptr);
    EXPECT_NE(reg.find("LowPass"),      nullptr);
    EXPECT_NE(reg.find("CastFloatInt"), nullptr);
    EXPECT_NE(reg.find("CastIntFloat"), nullptr);
    EXPECT_NE(reg.find("ProbeFloat"),   nullptr);
    EXPECT_NE(reg.find("ProbeInt"),     nullptr);

    EXPECT_GE(reg.size(), 8u);
}

TEST(BuiltinNodes, AddTypeHasExpectedAttrs)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    auto const* add = reg.find("Add");
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
    auto const* nt = reg.find("SinWave");
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
    auto const* nt = reg.find("LowPass");
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
    auto const* probe_f = reg.find("ProbeFloat");
    ASSERT_NE(probe_f, nullptr);
    ASSERT_EQ(probe_f->attributes.size(), 1u);
    EXPECT_EQ(probe_f->attributes[0].role, AttributeSpec::Role::Input);
    EXPECT_EQ(probe_f->attributes[0].data_type, "float");

    auto const* probe_i = reg.find("ProbeInt");
    ASSERT_NE(probe_i, nullptr);
    EXPECT_EQ(probe_i->attributes[0].data_type, "int");
}

TEST(BuiltinNodes, CastsHaveOpposingTypes)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    auto const* fi = reg.find("CastFloatInt");
    ASSERT_NE(fi, nullptr);
    EXPECT_EQ(fi->attributes[0].data_type, "float");
    EXPECT_EQ(fi->attributes[1].data_type, "int");

    auto const* if_ = reg.find("CastIntFloat");
    ASSERT_NE(if_, nullptr);
    EXPECT_EQ(if_->attributes[0].data_type, "int");
    EXPECT_EQ(if_->attributes[1].data_type, "float");
}
