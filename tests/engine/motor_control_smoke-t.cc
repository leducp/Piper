#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/node.h"
#include "piper/registry.h"
#include "piper/serialize_v2.h"

#include "piper/engine/builtin_steps.h"
#include "piper/engine/engine.h"
#include "piper/engine/external_io.h"
#include "piper/engine/registry.h"

using piper::Graph;
using piper::Node;
using piper::NodeRegistry;
using piper::engine::Engine;
using piper::engine::step::Output;
using piper::engine::Stage;
using piper::engine::StepRegistry;

namespace piper_engine_test
{
    std::string read_file(std::string const& path)
    {
        std::ifstream in(path);
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    piper::NodeId find_node_by_type(Graph const& g, std::string const& type)
    {
        for (auto const& n : g.nodes())
        {
            if (n.type == type)
            {
                return n.id;
            }
        }
        return piper::invalid_node_id;
    }
}

TEST(EngineSmoke, EngineDemoRunsToFiniteOutput)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    auto const path = std::string{ PIPER_SOURCE_DIR } + "/examples/engine_demo.piper";
    auto const json = piper_engine_test::read_file(path);
    ASSERT_FALSE(json.empty()) << "could not read " << path;

    auto const lr = piper::v2::deserialize(json, nr);
    Graph const& g = lr.graph;
    ASSERT_FALSE(g.nodes().empty());

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok) << (res.diagnostics.empty() ? "" : res.diagnostics.front().message);

    for (int i = 0; i < 1000; ++i)
    {
        e.play();
    }

    auto probe_id = piper_engine_test::find_node_by_type(g, "external_output<float>");
    ASSERT_NE(probe_id, piper::invalid_node_id);
    auto* probe = dynamic_cast<Output<float>*>(e.step_for(probe_id));
    ASSERT_NE(probe, nullptr);
    EXPECT_TRUE(std::isfinite(probe->get()));
    // sin_wave at 1 Hz, amplitude 1.0, low-passed at 10 Hz cutoff —
    // amplitude is preserved well under unity, so the probe stays
    // bounded by ~1.5 in steady state.
    EXPECT_LT(std::abs(probe->get()), 1.5f);
}
