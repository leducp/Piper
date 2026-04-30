#include <gtest/gtest.h>

#include "piper/connect.h"
#include "piper/graph.h"
#include "piper/type_check.h"

#include "test_helpers.h"

using namespace piper;
using piper::fixtures::make_adder;

NodeType make_caster()
{
    NodeType nt;
    nt.type = "Cast";
    nt.attributes = {
        { "in",  "int32_t",   AttributeSpec::Role::Input,  "" },
        { "out", "float", AttributeSpec::Role::Output, "" },
    };
    return nt;
}

// ---- TypeCheck default policy ----

TEST(TypeCheck, DefaultIsStringEquality)
{
    TypeCheck tc;
    EXPECT_TRUE(tc.compatible("float", "float"));
    EXPECT_TRUE(tc.compatible("vec3", "vec3"));
    EXPECT_FALSE(tc.compatible("float", "int"));
    EXPECT_FALSE(tc.compatible("vec3", "vec4"));
}

TEST(TypeCheck, EmptyStringsCompareEqual)
{
    TypeCheck tc;
    EXPECT_TRUE(tc.compatible("", ""));
    EXPECT_FALSE(tc.compatible("", "float"));
}

class PromotingTypeCheck : public TypeCheck
{
public:
    bool compatible(std::string_view a, std::string_view b) const override
    {
        if (a == b)
        {
            return true;
        }
        if (a == "int32_t" and b == "float")
        {
            return true;
        }
        return false;
    }
};

TEST(TypeCheck, SubclassOverrideUsed)
{
    PromotingTypeCheck tc;
    EXPECT_TRUE(tc.compatible("int32_t", "float"));
    EXPECT_FALSE(tc.compatible("float", "int"));
    EXPECT_TRUE(tc.compatible("float", "float"));
}

// ---- validate_connection: every Connect value ----

TEST(ValidateConnection, AllowsCompatiblePins)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    auto b = g.add_node(adder, "b", "control", {});
    TypeCheck tc;

    EXPECT_EQ(validate_connection(g, { a, "out" }, { b, "a" }, tc),
              Connect::Allow);
}

TEST(ValidateConnection, UnknownNodeYieldsUnknownPin)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    TypeCheck tc;

    EXPECT_EQ(validate_connection(g, { 9999, "out" }, { a, "a" }, tc),
              Connect::UnknownPin);
    EXPECT_EQ(validate_connection(g, { a, "out" }, { 9999, "a" }, tc),
              Connect::UnknownPin);
}

TEST(ValidateConnection, UnknownAttrYieldsUnknownPin)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    auto b = g.add_node(adder, "b", "control", {});
    TypeCheck tc;

    EXPECT_EQ(validate_connection(g, { a, "no_such" }, { b, "a" }, tc),
              Connect::UnknownPin);
    EXPECT_EQ(validate_connection(g, { a, "out" }, { b, "no_such" }, tc),
              Connect::UnknownPin);
}

TEST(ValidateConnection, SameNodeRejected)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    TypeCheck tc;

    EXPECT_EQ(validate_connection(g, { a, "out" }, { a, "a" }, tc),
              Connect::SameNode);
}

TEST(ValidateConnection, KindMismatchOnInputAsSource)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    auto b = g.add_node(adder, "b", "control", {});
    TypeCheck tc;

    EXPECT_EQ(validate_connection(g, { a, "a" }, { b, "a" }, tc),
              Connect::KindMismatch);
}

TEST(ValidateConnection, SameNodeWinsOverKindMismatch)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    TypeCheck tc;

    // out -> out on the same node -- SameNode must be reported, not KindMismatch.
    EXPECT_EQ(validate_connection(g, { a, "out" }, { a, "out" }, tc),
              Connect::SameNode);
    // a -> a on the same node -- same priority.
    EXPECT_EQ(validate_connection(g, { a, "a" }, { a, "a" }, tc),
              Connect::SameNode);
}

TEST(ValidateConnection, KindMismatchOnOutputAsDestination)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    auto b = g.add_node(adder, "b", "control", {});
    TypeCheck tc;

    // to is Output -- wrong direction
    EXPECT_EQ(validate_connection(g, { a, "out" }, { b, "out" }, tc),
              Connect::KindMismatch);
}

TEST(ValidateConnection, KindMismatchOnMember)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    auto b = g.add_node(adder, "b", "control", {});
    TypeCheck tc;

    // Member role on either endpoint
    EXPECT_EQ(validate_connection(g, { a, "k" }, { b, "a" }, tc),
              Connect::KindMismatch);
    EXPECT_EQ(validate_connection(g, { a, "out" }, { b, "k" }, tc),
              Connect::KindMismatch);
}

TEST(ValidateConnection, TypeMismatchWithDefaultPolicy)
{
    Graph g;
    auto adder = make_adder();
    auto caster = make_caster();
    auto a = g.add_node(caster, "a", "control", {});  // out is "float"
    auto b = g.add_node(adder,  "b", "control", {});  // a   is "float"  (compatible)
    auto c = g.add_node(caster, "c", "control", {});  // in  is "int"
    TypeCheck tc;

    // float -> float: Allow
    EXPECT_EQ(validate_connection(g, { a, "out" }, { b, "a" }, tc),
              Connect::Allow);

    // float -> int: TypeMismatch with default policy
    EXPECT_EQ(validate_connection(g, { a, "out" }, { c, "in" }, tc),
              Connect::TypeMismatch);
}

TEST(ValidateConnection, TypePromotionAllowedBySubclass)
{
    NodeType int_source;
    int_source.type = "IntSource";
    int_source.attributes = {
        { "out", "int32_t", AttributeSpec::Role::Output, "" },
    };

    NodeType float_sink;
    float_sink.type = "FloatSink";
    float_sink.attributes = {
        { "in", "float", AttributeSpec::Role::Input, "" },
    };

    Graph g;
    auto src = g.add_node(int_source, "src", "control", {});
    auto dst = g.add_node(float_sink, "dst", "control", {});

    // Default policy rejects int -> float.
    TypeCheck strict;
    EXPECT_EQ(validate_connection(g, { src, "out" }, { dst, "in" }, strict),
              Connect::TypeMismatch);

    // Promoting policy accepts int -> float.
    PromotingTypeCheck promoting;
    EXPECT_EQ(validate_connection(g, { src, "out" }, { dst, "in" }, promoting),
              Connect::Allow);
}

TEST(ValidateConnection, AlreadyConnectedRejected)
{
    Graph g;
    auto adder = make_adder();
    auto a = g.add_node(adder, "a", "control", {});
    auto b = g.add_node(adder, "b", "control", {});
    auto c = g.add_node(adder, "c", "control", {});
    TypeCheck tc;

    // First link occupies b.a
    auto lid = g.add_link({ a, "out" }, { b, "a" }, "float");
    ASSERT_NE(lid, invalid_link_id);

    // Second attempt at b.a: rejected
    EXPECT_EQ(validate_connection(g, { c, "out" }, { b, "a" }, tc),
              Connect::AlreadyConnected);

    // But b.b is still free
    EXPECT_EQ(validate_connection(g, { c, "out" }, { b, "b" }, tc),
              Connect::Allow);
}
