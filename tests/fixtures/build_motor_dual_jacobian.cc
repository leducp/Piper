#include "build_motor_dual_jacobian.h"

#include <stdexcept>

namespace piper::fixtures
{
    Graph build_motor_dual_jacobian(NodeRegistry const& reg)
    {
        Graph g;

        // Two stages: command/setpoint flow lives in "control",
        // measured pose comes back in "feedback". The motor node's
        // measured pin lives only in the feedback stage via a
        // per-pin override -- this is the canonical Bus pattern
        // where one node's pins span multiple stages.
        g.add_stage({ "control",  rgba::from_components(0xFF, 0x60, 0x60, 0xFF) });
        g.add_stage({ "feedback", rgba::from_components(0x60, 0xFF, 0x60, 0xFF) });

        auto require = [&](char const* type_name)
        {
            NodeType const* nt = reg.find(type_name);
            if (nt == nullptr)
            {
                throw std::runtime_error(
                    std::string{"registry missing '"} + type_name + "'");
            }
            return nt;
        };

        NodeType const* constant_f = require("constant<float>");
        NodeType const* jacobian   = require("jacobian_2x2");
        NodeType const* motor      = require("motor");
        NodeType const* probe_f    = require("probe<float>");

        auto target_x = g.add_node(*constant_f, "target_x", "control",
                                   Point{   60.0f, 100.0f });
        auto target_y = g.add_node(*constant_f, "target_y", "control",
                                   Point{   60.0f, 240.0f });
        auto jac      = g.add_node(*jacobian,   "jacobian", "control",
                                   Point{  320.0f, 160.0f });
        auto motor_a  = g.add_node(*motor,      "motor_a",  "control",
                                   Point{  600.0f, 100.0f });
        auto motor_b  = g.add_node(*motor,      "motor_b",  "control",
                                   Point{  600.0f, 240.0f });
        auto pose_a   = g.add_node(*probe_f,    "pose_a",   "feedback",
                                   Point{  860.0f, 100.0f });
        auto pose_b   = g.add_node(*probe_f,    "pose_b",   "feedback",
                                   Point{  860.0f, 240.0f });

        // The motor's `measured` output runs only in the feedback
        // stage even though the rest of the motor (and its `command`
        // input) lives in control. Per-pin override carries this.
        std::vector<std::string> const fb_only{ "feedback" };
        g.set_attr_stages(motor_a, "measured", fb_only);
        g.set_attr_stages(motor_b, "measured", fb_only);

        // Tune target setpoints so the example loads with non-zero
        // values when an inspector is opened on the constants.
        g.set_attr_value(target_x, "value", "1.0");
        g.set_attr_value(target_y, "value", "0.0");

        // Identity-ish jacobian by default; user tweaks via the
        // inspector. j00=1, j01=0, j10=0, j11=1 are the spec
        // defaults so no per-instance override is required.

        // Setpoints fan into the jacobian, jacobian into motor
        // commands, motor measurements out to the probes.
        g.add_link({ target_x, "out"      }, { jac,     "in_a"    }, "float");
        g.add_link({ target_y, "out"      }, { jac,     "in_b"    }, "float");
        g.add_link({ jac,      "out_a"    }, { motor_a, "command" }, "float");
        g.add_link({ jac,      "out_b"    }, { motor_b, "command" }, "float");
        g.add_link({ motor_a,  "measured" }, { pose_a,  "in"      }, "float");
        g.add_link({ motor_b,  "measured" }, { pose_b,  "in"      }, "float");

        ModeProfile profile;
        profile.name              = "default";
        profile.is_default        = true;
        profile.per_node[target_x] = "enable";
        profile.per_node[target_y] = "enable";
        profile.per_node[jac]      = "enable";
        profile.per_node[motor_a]  = "enable";
        profile.per_node[motor_b]  = "enable";
        profile.per_node[pose_a]   = "enable";
        profile.per_node[pose_b]   = "enable";
        g.add_mode_profile(profile);

        return g;
    }
}
