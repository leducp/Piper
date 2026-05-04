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

        auto target_x = g.add_node(*constant_f, "target_x", "control",  Point{  60.0f, 100.0f });
        auto target_y = g.add_node(*constant_f, "target_y", "control",  Point{  60.0f, 240.0f });
        auto jac      = g.add_node(*jacobian,   "jacobian", "control",  Point{ 320.0f, 160.0f });
        auto motor_a  = g.add_node(*motor,      "motor_a",  "control",  Point{ 600.0f, 100.0f });
        auto motor_b  = g.add_node(*motor,      "motor_b",  "control",  Point{ 600.0f, 240.0f });
        auto pose_a   = g.add_node(*probe_f,    "pose_a",   "feedback", Point{ 860.0f, 100.0f });
        auto pose_b   = g.add_node(*probe_f,    "pose_b",   "feedback", Point{ 860.0f, 240.0f });

        // Bus pattern: each motor primarily lives in "control" but its
        // measured-output pin reports back in "feedback".
        g.set_attr_stages(motor_a, "measured", { "feedback" });
        g.set_attr_stages(motor_b, "measured", { "feedback" });

        g.set_attr_value(target_x, "value", "1.0");
        g.set_attr_value(target_y, "value", "0.0");

        g.add_link(PinRef{ target_x, "out"      }, PinRef{ jac,     "in_a"    }, "float");
        g.add_link(PinRef{ target_y, "out"      }, PinRef{ jac,     "in_b"    }, "float");
        g.add_link(PinRef{ jac,      "out_a"    }, PinRef{ motor_a, "command" }, "float");
        g.add_link(PinRef{ jac,      "out_b"    }, PinRef{ motor_b, "command" }, "float");
        g.add_link(PinRef{ motor_a,  "measured" }, PinRef{ pose_a,  "in"      }, "float");
        g.add_link(PinRef{ motor_b,  "measured" }, PinRef{ pose_b,  "in"      }, "float");

        ModeProfile profile;
        profile.name              = "default";
        profile.per_node[target_x] = "enable";
        profile.per_node[target_y] = "enable";
        profile.per_node[jac]      = "enable";
        profile.per_node[motor_a]  = "enable";
        profile.per_node[motor_b]  = "enable";
        profile.per_node[pose_a]   = "enable";
        profile.per_node[pose_b]   = "enable";
        g.add_mode_profile(profile);
        g.set_default_mode_name("default");

        return g;
    }
}
