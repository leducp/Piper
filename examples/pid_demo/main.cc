#include <cmath>
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
    constexpr double tick_period = 0.001;   // matches the engine's hardcoded rate
    constexpr int    tick_count  = 4000;    // 4 s of simulation
    constexpr int    mode_period = 1000;    // switch modes every 1 s

    char const* mode_at(int tick)
    {
        int const phase = (tick / mode_period) % 4;
        if (phase == 0) { return "tight"; }
        if (phase == 1) { return "loose"; }
        if (phase == 2) { return "bypass"; }
        return "tight";
    }

    int mode_index(char const* name)
    {
        std::string const s{ name };
        if (s == "tight")  { return 0; }
        if (s == "loose")  { return 1; }
        if (s == "bypass") { return 2; }
        return -1;
    }

    // Square setpoint, period 2 s, amplitude 1.
    float square_setpoint(double t)
    {
        double const phase = std::fmod(t, 2.0);
        if (phase < 1.0) { return 1.0f; }
        return -1.0f;
    }
}

int main(int argc, char** argv)
{
    std::string const default_pipeline =
        std::string{ PIPER_SOURCE_DIR } + "/examples/pid_demo/pid_demo.piper";
    std::string pipeline_path = (argc >= 2) ? argv[1] : default_pipeline;
    std::string csv_path      = (argc >= 3) ? argv[2] : "pid_demo.csv";

    std::ifstream in(pipeline_path);
    if (not in)
    {
        std::fprintf(stderr, "pid_demo: could not open %s\n", pipeline_path.c_str());
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
        std::fprintf(stderr, "pid_demo: build failed (%zu diagnostics)\n",
                     build.diagnostics.size());
        for (auto const& d : build.diagnostics)
        {
            std::fprintf(stderr, "  - %s\n", d.message.c_str());
        }
        return 1;
    }

    // Host-driven setpoint and probe handles, all looked up by name.
    auto*       command        = engine.input<float>("command");
    auto const* probe_command  = engine.output<float>("probe_command");
    auto const* probe_pid_out  = engine.output<float>("probe_pid_out");
    auto const* probe_measured = engine.output<float>("probe_measured");
    if (not (command and probe_command and probe_pid_out and probe_measured))
    {
        std::fprintf(stderr, "pid_demo: missing 'command' input or a probe "
                             "(check 'name' members on the externals)\n");
        return 1;
    }

    using Probe = std::pair<char const*, piper::engine::step::Output<float> const*>;
    Probe const probes[] = {
        { "probe_command",  probe_command  },
        { "probe_pid_out",  probe_pid_out  },
        { "probe_measured", probe_measured },
    };

    std::ofstream csv(csv_path);
    if (not csv)
    {
        std::fprintf(stderr, "pid_demo: could not open %s for writing\n", csv_path.c_str());
        return 1;
    }
    csv << "tick,t,mode";
    for (auto const& [name, _] : probes)
    {
        csv << ',' << name;
    }
    csv << '\n';

    std::printf("pid_demo: loaded %s\n", pipeline_path.c_str());
    std::printf("  probes: %zu\n", std::size(probes));
    std::printf("  modes : tight(0) loose(1) bypass(2), 1 s each\n");

    char const* current_mode = nullptr;
    for (int i = 0; i < tick_count; ++i)
    {
        char const* desired = mode_at(i);
        if (desired != current_mode)
        {
            engine.set_mode(desired);
            current_mode = desired;
        }

        double const t = static_cast<double>(i) * tick_period;
        command->set(square_setpoint(t));

        engine.play();

        csv << i << ',' << t << ',' << mode_index(current_mode);
        for (auto const& [_, p] : probes)
        {
            csv << ',' << p->get();
        }
        csv << '\n';
    }

    std::printf("pid_demo: wrote %d samples to %s\n", tick_count, csv_path.c_str());
    return 0;
}
