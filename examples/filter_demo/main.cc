#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/registry.h"
#include "piper/serialize_v2.h"

#include "piper/engine/builtin_steps.h"
#include "piper/engine/engine.h"
#include "piper/engine/external_io.h"
#include "piper/engine/registry.h"

namespace
{
    constexpr double tick_period = 0.001;
    constexpr int    tick_count  = 4000;
}

int main(int argc, char** argv)
{
    std::string const default_pipeline =
        std::string{ PIPER_SOURCE_DIR } + "/examples/filter_demo/filter_demo.piper";
    std::string pipeline_path = (argc >= 2) ? argv[1] : default_pipeline;
    std::string csv_path      = (argc >= 3) ? argv[2] : "filter_demo.csv";

    std::ifstream in(pipeline_path);
    if (not in)
    {
        std::fprintf(stderr, "filter_demo: could not open %s\n", pipeline_path.c_str());
        return 1;
    }
    std::ostringstream buf;
    buf << in.rdbuf();

    piper::NodeRegistry node_reg;
    piper::register_builtin_nodes(node_reg);
    auto const lr = piper::v2::deserialize(buf.str(), node_reg);

    piper::engine::StepRegistry step_reg;
    piper::engine::register_builtin_steps(step_reg);

    piper::engine::Engine engine;
    auto const build = engine.build(lr.graph, step_reg);
    if (not build.ok)
    {
        std::fprintf(stderr, "filter_demo: build failed (%zu diagnostics)\n",
                     build.diagnostics.size());
        for (auto const& d : build.diagnostics)
        {
            std::fprintf(stderr, "  - %s\n", d.message.c_str());
        }
        return 1;
    }

    // Probe handles by name. Each external_output<float> in the .piper
    // carries a "name" Member that this lookup matches against.
    auto const* probe_raw      = engine.output<float>("probe_raw");
    auto const* probe_filtered = engine.output<float>("probe_filtered");
    if (not (probe_raw and probe_filtered))
    {
        std::fprintf(stderr, "filter_demo: missing probe_raw or probe_filtered "
                             "(check the 'name' member on each external_output)\n");
        return 1;
    }

    using Probe = std::pair<char const*, piper::engine::step::Output<float> const*>;
    Probe const probes[] = {
        { "probe_raw",      probe_raw      },
        { "probe_filtered", probe_filtered },
    };

    std::ofstream csv(csv_path);
    if (not csv)
    {
        std::fprintf(stderr, "filter_demo: could not open %s for writing\n", csv_path.c_str());
        return 1;
    }
    csv << "tick,t";
    for (auto const& [name, _] : probes)
    {
        csv << ',' << name;
    }
    csv << '\n';

    std::printf("filter_demo: loaded %s\n", pipeline_path.c_str());
    std::printf("  probes: %zu\n", std::size(probes));

    for (int i = 0; i < tick_count; ++i)
    {
        engine.play();
        csv << i << ',' << (static_cast<double>(i) * tick_period);
        for (auto const& [_, p] : probes)
        {
            csv << ',' << p->get();
        }
        csv << '\n';
    }

    std::printf("filter_demo: wrote %d samples to %s\n", tick_count, csv_path.c_str());
    return 0;
}
