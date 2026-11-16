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
#include "piper/engine/registry.h"

namespace
{
    constexpr double tick_period = 0.001;
    constexpr int    tick_count  = 4000;
}

int main(int argc, char** argv)
{
    std::string const default_pipeline =
        std::string{ PIPER_SOURCE_DIR } + "/examples/am_radio/am_radio.piper";
    std::string pipeline_path = default_pipeline;
    if (argc >= 2)
    {
        pipeline_path = argv[1];
    }
    std::string csv_path = "am_radio.csv";
    if (argc >= 3)
    {
        csv_path = argv[2];
    }

    std::ifstream in(pipeline_path);
    if (not in)
    {
        std::fprintf(stderr, "am_radio: could not open %s\n", pipeline_path.c_str());
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
        std::fprintf(stderr, "am_radio: build failed (%zu diagnostics)\n",
                     build.diagnostics.size());
        for (auto const& d : build.diagnostics)
        {
            std::fprintf(stderr, "  - %s\n", d.message.c_str());
        }
        return 1;
    }

    // Probe names come from the external output attributes in the .piper file.
    auto const* probe_envelope    = engine.output<float>("probe_envelope");
    auto const* probe_carrier     = engine.output<float>("probe_carrier");
    auto const* probe_modulated   = engine.output<float>("probe_modulated");
    auto const* probe_transmitted = engine.output<float>("probe_transmitted");
    auto const* probe_rectified   = engine.output<float>("probe_rectified");
    auto const* probe_recovered   = engine.output<float>("probe_recovered");
    if (not (probe_envelope and probe_carrier and probe_modulated
             and probe_transmitted and probe_rectified and probe_recovered))
    {
        std::fprintf(stderr, "am_radio: a probe is missing (check the 'value' "
                             "external name on every external_output)\n");
        return 1;
    }

    using Probe = std::pair<char const*, float const*>;
    Probe const probes[] = {
        { "probe_envelope",    probe_envelope    },
        { "probe_carrier",     probe_carrier     },
        { "probe_modulated",   probe_modulated   },
        { "probe_transmitted", probe_transmitted },
        { "probe_rectified",   probe_rectified   },
        { "probe_recovered",   probe_recovered   },
    };

    std::ofstream csv(csv_path);
    if (not csv)
    {
        std::fprintf(stderr, "am_radio: could not open %s for writing\n", csv_path.c_str());
        return 1;
    }
    csv << "tick,t";
    for (auto const& [name, _] : probes)
    {
        csv << ',' << name;
    }
    csv << '\n';

    std::printf("am_radio: loaded %s\n", pipeline_path.c_str());
    std::printf("  probes: %zu\n", std::size(probes));

    for (int i = 0; i < tick_count; ++i)
    {
        engine.play();
        csv << i << ',' << (static_cast<double>(i) * tick_period);
        for (auto const& [_, p] : probes)
        {
            csv << ',' << *p;
        }
        csv << '\n';
    }

    std::printf("am_radio: wrote %d samples to %s\n", tick_count, csv_path.c_str());
    return 0;
}
