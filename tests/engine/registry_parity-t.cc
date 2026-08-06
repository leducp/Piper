#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "piper/builtin_nodes.h"
#include "piper/registry.h"

#include "piper/engine/builtin_steps.h"
#include "piper/engine/registry.h"

using namespace piper;

// Node types deliberately registered without a Step factory: probes
// are editor-side inspection sinks, and the example::* pair
// illustrates the catalog format for the walkthrough.
bool parity_test_has_no_engine_impl(std::string const& type)
{
    return type.starts_with("probe<")
        or type == "jacobian_2x2"
        or type == "motor";
}

// The two registries are populated by separate folds over
// piper::BuiltinScalars. This is what catches a family added to one
// side and not the other -- the drift that left double without a
// constant<> and uint32_t without anything at all.
TEST(RegistryParity, EveryNodeTypeHasAStepFactory)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    engine::StepRegistry sr;
    engine::register_builtin_steps(sr);

    std::size_t no_impl = 0;
    for (auto const* nt : reg.all())
    {
        if (parity_test_has_no_engine_impl(nt->type))
        {
            ++no_impl;
            continue;
        }
        EXPECT_NE(sr.find(nt->type), nullptr)
            << "node type '" << nt->type << "' has no step factory";
    }

    EXPECT_GT(no_impl, 0u);
    // Reverse direction: no factory registered under a name the node
    // registry does not advertise.
    EXPECT_EQ(sr.size(), reg.size() - no_impl);
}

TEST(RegistryParity, ScalarFamiliesCoverEveryWidth)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    static constexpr char const* scalars[] = {
        "float", "double",
        "int8_t", "int16_t", "int32_t", "int64_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    };
    static constexpr char const* families[] = {
        "constant", "add", "subtract", "multiply",
        "mux3", "clamp", "preset3",
        "external_input", "external_output", "probe",
    };

    for (char const* s : scalars)
    {
        for (char const* f : families)
        {
            std::string const type = std::string(f) + "<" + s + ">";
            EXPECT_NE(reg.find(type), nullptr) << "missing " << type;
        }
    }
}

TEST(RegistryParity, AbsIsSignedOnlyAndTimeSteppedIsFloatOnly)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    EXPECT_NE(reg.find("abs<int32_t>"), nullptr);
    EXPECT_NE(reg.find("abs<double>"),  nullptr);
    EXPECT_EQ(reg.find("abs<uint32_t>"), nullptr);
    EXPECT_EQ(reg.find("abs<uint8_t>"),  nullptr);

    EXPECT_NE(reg.find("pid<float>"),      nullptr);
    EXPECT_NE(reg.find("pid<double>"),     nullptr);
    EXPECT_EQ(reg.find("pid<int32_t>"),    nullptr);
    EXPECT_EQ(reg.find("sin_wave<int32_t>"), nullptr);
    EXPECT_EQ(reg.find("low_pass<uint64_t>"), nullptr);
}

TEST(RegistryParity, CastCoversEveryOrderedScalarPair)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    static constexpr char const* scalars[] = {
        "float", "double",
        "int8_t", "int16_t", "int32_t", "int64_t",
        "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    };

    std::size_t pairs = 0;
    for (char const* from : scalars)
    {
        for (char const* to : scalars)
        {
            std::string const type =
                std::string("cast<") + from + "," + to + ">";
            if (std::string_view(from) == to)
            {
                EXPECT_EQ(reg.find(type), nullptr) << "identity " << type;
                continue;
            }
            EXPECT_NE(reg.find(type), nullptr) << "missing " << type;
            ++pairs;
        }
    }
    EXPECT_EQ(pairs, 90u);
}

// Unsigned min/max defaults must not be negative: the step's member
// parser saturates a negative literal to 0, so a "-100" default would
// disagree with what the inspector shows.
TEST(RegistryParity, UnsignedRangeDefaultsAreNonNegative)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    for (char const* type : { "clamp<uint32_t>", "external_input<uint16_t>" })
    {
        auto const* nt = reg.find(type);
        ASSERT_NE(nt, nullptr) << type;
        for (auto const& a : nt->attributes)
        {
            if (a.name == "min" or a.name == "max")
            {
                EXPECT_EQ(a.default_value.find('-'), std::string::npos)
                    << type << " ." << a.name << " = " << a.default_value;
            }
        }
    }
}
