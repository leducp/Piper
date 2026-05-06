#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>

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

std::string engine_demo_path()
{
    return std::string(PIPER_SOURCE_DIR) + "/examples/engine_demo.piper";
}

TEST(Example, EngineDemoLoadsCleanly)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    std::string text = read_file(engine_demo_path());
    ASSERT_FALSE(text.empty()) << "missing examples/engine_demo.piper";

    auto loaded = v2::deserialize(text, reg);
    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_EQ(loaded.graph.nodes().size(),         6u);
    EXPECT_EQ(loaded.graph.links().size(),         5u);
    EXPECT_EQ(loaded.graph.stages().size(),        3u);
    EXPECT_EQ(loaded.graph.mode_profiles().size(), 1u);
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
    EXPECT_EQ(loaded.graph.stages().size(),        3u);
    EXPECT_EQ(loaded.graph.mode_profiles().size(), 1u);
}
