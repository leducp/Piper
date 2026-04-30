#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/node.h"
#include "piper/registry.h"
#include "piper/serialize_v2.h"

#include "piper/engine/builtin_steps.h"
#include "piper/engine/engine.h"
#include "piper/engine/registry.h"
#include "piper/engine/step.h"

int main(int argc, char** argv)
{
    std::string path;
    if (argc >= 2)
    {
        path = argv[1];
    }
    else
    {
        path = std::string{ PIPER_SOURCE_DIR } + "/examples/motor_control_simple.piper";
    }

    std::ifstream in(path);
    if (not in)
    {
        std::fprintf(stderr, "engine_demo: could not open %s\n", path.c_str());
        return 1;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    std::string const json = buf.str();

    piper::NodeRegistry node_reg;
    piper::register_builtin_nodes(node_reg);

    auto const lr = piper::v2::deserialize(json, node_reg);
    if (lr.graph.nodes().empty())
    {
        std::fprintf(stderr, "engine_demo: no nodes loaded from %s\n", path.c_str());
        return 1;
    }

    piper::engine::StepRegistry step_reg;
    piper::engine::register_builtin_steps(step_reg);

    piper::engine::Engine engine;
    auto build = engine.build(lr.graph, step_reg);
    if (not build.ok)
    {
        std::fprintf(stderr, "engine_demo: build failed (%zu diagnostics)\n",
                     build.diagnostics.size());
        for (auto const& d : build.diagnostics)
        {
            std::fprintf(stderr, "  - %s\n", d.message.c_str());
        }
        return 1;
    }

    piper::NodeId probe_id = piper::invalid_node_id;
    for (auto const& n : lr.graph.nodes())
    {
        if (n.type == "external_output<float>")
        {
            probe_id = n.id;
            break;
        }
    }

    std::printf("engine_demo: loaded %s\n", path.c_str());
    std::printf("  nodes:  %zu\n",  lr.graph.nodes().size());
    std::printf("  links:  %zu\n",  lr.graph.links().size());
    std::printf("  stages: %zu\n",  lr.graph.stages().size());

    auto const* probe = engine.step_for(probe_id);
    int const ticks = 1000;
    for (int i = 0; i < ticks; ++i)
    {
        engine.play();
        if (probe != nullptr and (i % 100) == 0)
        {
            std::printf("  tick %4d  probe=%.6f\n", i, probe->input<float>("in"));
        }
    }
    return 0;
}
