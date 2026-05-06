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

std::string filter_demo_path()
{
    return std::string(PIPER_SOURCE_DIR) +
           "/examples/filter_demo/filter_demo.piper";
}

std::string am_radio_path()
{
    return std::string(PIPER_SOURCE_DIR) +
           "/examples/am_radio/am_radio.piper";
}

std::string pid_demo_path()
{
    return std::string(PIPER_SOURCE_DIR) +
           "/examples/pid_demo/pid_demo.piper";
}

std::string dual_example_path()
{
    return std::string(PIPER_SOURCE_DIR) +
           "/examples/motor_control_dual_jacobian/motor_control_dual_jacobian.piper";
}

TEST(Example, FilterDemoLoadsCleanly)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    std::string text = read_file(filter_demo_path());
    ASSERT_FALSE(text.empty()) << "missing " << filter_demo_path();

    auto loaded = v2::deserialize(text, reg);
    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_EQ(loaded.graph.nodes().size(),         6u);
    EXPECT_EQ(loaded.graph.links().size(),         5u);
    EXPECT_EQ(loaded.graph.stages().size(),        3u);
    EXPECT_EQ(loaded.graph.mode_profiles().size(), 1u);
}

TEST(Example, AmRadioLoadsCleanly)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    std::string text = read_file(am_radio_path());
    ASSERT_FALSE(text.empty()) << "missing " << am_radio_path();

    auto loaded = v2::deserialize(text, reg);
    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_EQ(loaded.graph.nodes().size(),         15u);
    EXPECT_EQ(loaded.graph.links().size(),         14u);
    EXPECT_EQ(loaded.graph.stages().size(),        3u);
    EXPECT_EQ(loaded.graph.mode_profiles().size(), 1u);
}

TEST(Example, PidDemoLoadsCleanly)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    std::string text = read_file(pid_demo_path());
    ASSERT_FALSE(text.empty()) << "missing " << pid_demo_path();

    auto loaded = v2::deserialize(text, reg);
    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_EQ(loaded.graph.nodes().size(),         9u);
    EXPECT_EQ(loaded.graph.links().size(),         9u);
    EXPECT_EQ(loaded.graph.stages().size(),        3u);
    EXPECT_EQ(loaded.graph.mode_profiles().size(), 3u);
}

TEST(Example, MotorControlDualJacobianLoadsCleanly)
{
    NodeRegistry reg;
    register_builtin_nodes(reg);

    std::string text = read_file(dual_example_path());
    ASSERT_FALSE(text.empty()) << "missing " << dual_example_path();

    auto loaded = v2::deserialize(text, reg);
    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_EQ(loaded.graph.nodes().size(),         7u);
    EXPECT_EQ(loaded.graph.links().size(),         6u);
    EXPECT_EQ(loaded.graph.stages().size(),        3u);
    EXPECT_EQ(loaded.graph.mode_profiles().size(), 1u);
}
