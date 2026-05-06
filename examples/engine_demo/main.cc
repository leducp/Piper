#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/node.h"
#include "piper/registry.h"
#include "piper/serialize_v2.h"

#include "piper/engine/builtin_steps.h"
#include "piper/engine/engine.h"
#include "piper/engine/registry.h"
#include "piper/engine/step.h"

namespace
{
    constexpr double tick_period = 0.001;
    constexpr int    tick_count  = 4000;
}

int main(int argc, char** argv)
{
    std::string pipeline_path;
    if (argc >= 2)
    {
        pipeline_path = argv[1];
    }
    else
    {
        pipeline_path = std::string{ PIPER_SOURCE_DIR } + "/examples/engine_demo.piper";
    }

    std::string csv_path = (argc >= 3) ? argv[2] : "engine_demo.csv";

    std::ifstream in(pipeline_path);
    if (not in)
    {
        std::fprintf(stderr, "engine_demo: could not open %s\n", pipeline_path.c_str());
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
        std::fprintf(stderr, "engine_demo: no nodes loaded from %s\n", pipeline_path.c_str());
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

    std::vector<std::pair<std::string, piper::engine::Step const*>> probes;
    for (auto const& n : lr.graph.nodes())
    {
        if (n.type == "external_output<float>")
        {
            probes.emplace_back(n.name, engine.step_for(n.id));
        }
    }

    std::ofstream csv(csv_path);
    if (not csv)
    {
        std::fprintf(stderr, "engine_demo: could not open %s for writing\n", csv_path.c_str());
        return 1;
    }
    csv << "tick,t";
    for (auto const& [name, _] : probes)
    {
        csv << ',' << name;
    }
    csv << '\n';

    std::printf("engine_demo: loaded %s\n", pipeline_path.c_str());
    std::printf("  nodes:  %zu\n",  lr.graph.nodes().size());
    std::printf("  links:  %zu\n",  lr.graph.links().size());
    std::printf("  stages: %zu\n",  lr.graph.stages().size());
    std::printf("  probes: %zu\n",  probes.size());

    for (int i = 0; i < tick_count; ++i)
    {
        engine.play();
        csv << i << ',' << (static_cast<double>(i) * tick_period);
        for (auto const& [_, step] : probes)
        {
            csv << ',' << step->input<float>("in");
        }
        csv << '\n';
    }

    std::printf("engine_demo: wrote %d samples to %s\n", tick_count, csv_path.c_str());
    return 0;
}
