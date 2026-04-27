#include "build_motor_simple.h"

#include <stdexcept>

namespace piper::fixtures
{
    Graph build_motor_simple(NodeRegistry const& reg)
    {
        Graph g;

        g.add_stage({ "control",  rgba::from_components(0xFF, 0x60, 0x60, 0xFF) });
        g.add_stage({ "feedback", rgba::from_components(0x60, 0xFF, 0x60, 0xFF) });

        NodeType const* sin_wave = reg.find("SinWave");
        if (sin_wave == nullptr)
        {
            throw std::runtime_error("registry missing 'SinWave'");
        }
        NodeType const* low_pass = reg.find("LowPass");
        if (low_pass == nullptr)
        {
            throw std::runtime_error("registry missing 'LowPass'");
        }
        NodeType const* probe = reg.find("ProbeFloat");
        if (probe == nullptr)
        {
            throw std::runtime_error("registry missing 'ProbeFloat'");
        }

        auto target_id = g.add_node(*sin_wave, "joint_target", "control",  Point{ 100.0f, 100.0f });
        auto filter_id = g.add_node(*low_pass, "filter",       "control",  Point{ 280.0f, 100.0f });
        auto probe_id  = g.add_node(*probe,    "probe",        "feedback", Point{ 460.0f, 100.0f });

        g.add_link({ target_id, "out" }, { filter_id, "in" }, "float");
        g.add_link({ filter_id, "out" }, { probe_id,  "in" }, "float");

        ModeProfile profile;
        profile.name                 = "default";
        profile.is_default           = true;
        profile.per_node[target_id]  = "enable";
        profile.per_node[filter_id]  = "enable";
        profile.per_node[probe_id]   = "enable";
        g.add_mode_profile(profile);

        return g;
    }
}
