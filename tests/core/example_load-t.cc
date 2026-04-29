#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>

#include "../fixtures/build_motor_dual_jacobian.h"
#include "../fixtures/build_motor_simple.h"

#include "piper/builtin_nodes.h"
#include "piper/serialize_v2.h"

using namespace piper;

std::string read_file(std::string const& path)
{
    std::ifstream in{path};
    if (not in.is_open())
    {
        return {};
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

std::string example_path()
{
    return std::string(PIPER_SOURCE_DIR) + "/examples/motor_control_simple.piper";
}

TEST(Example, MotorControlSimpleLoadsCleanly)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    std::string text = read_file(example_path());
    ASSERT_FALSE(text.empty()) << "missing examples/motor_control_simple.piper";

    auto loaded = v2::deserialize(text, reg);
    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_EQ(loaded.graph.nodes().size(),         3u);
    EXPECT_EQ(loaded.graph.links().size(),         2u);
    EXPECT_EQ(loaded.graph.stages().size(),        2u);
    EXPECT_EQ(loaded.graph.mode_profiles().size(), 1u);
}

// Drift tripwire: if the generator and the committed file disagree, one
// of them is out of date. Regenerate via piper_build_motor_simple.
TEST(Example, MotorControlSimpleMatchesGenerator)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    Graph generated = fixtures::build_motor_simple(reg);
    std::string generated_text = v2::serialize(generated);

    std::string committed_text = read_file(example_path());
    ASSERT_FALSE(committed_text.empty());

    EXPECT_EQ(generated_text, committed_text)
        << "examples/motor_control_simple.piper is out of sync with the "
           "generator. Re-run: piper_build_motor_simple "
        << example_path();
}

std::string dual_example_path()
{
    return std::string(PIPER_SOURCE_DIR) +
           "/examples/motor_control_dual_jacobian.piper";
}

TEST(Example, MotorControlDualJacobianLoadsCleanly)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    std::string text = read_file(dual_example_path());
    ASSERT_FALSE(text.empty())
        << "missing examples/motor_control_dual_jacobian.piper";

    auto loaded = v2::deserialize(text, reg);
    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_EQ(loaded.graph.nodes().size(),         7u);
    EXPECT_EQ(loaded.graph.links().size(),         6u);
    EXPECT_EQ(loaded.graph.stages().size(),        2u);
    EXPECT_EQ(loaded.graph.mode_profiles().size(), 1u);
}

TEST(Example, MotorControlDualJacobianMatchesGenerator)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    Graph generated = fixtures::build_motor_dual_jacobian(reg);
    std::string generated_text = v2::serialize(generated);

    std::string committed_text = read_file(dual_example_path());
    ASSERT_FALSE(committed_text.empty());

    EXPECT_EQ(generated_text, committed_text)
        << "examples/motor_control_dual_jacobian.piper is out of sync "
           "with the generator. Re-run: piper_build_motor_dual_jacobian "
        << dual_example_path();
}
