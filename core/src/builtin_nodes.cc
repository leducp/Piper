#include "piper/builtin_nodes.h"

#include "piper/node_type.h"

namespace piper
{
    NodeType make_constant_float()
    {
        NodeType nt;
        nt.type     = "constant<float>";
        nt.library  = "math";
        nt.category = "constant";
        nt.help     = "Constant float source";
        nt.attributes = {
            { "value", "float", AttributeSpec::Role::Member, "0.0" },
            { "out",   "float", AttributeSpec::Role::Output, ""    },
        };
        return nt;
    }

    NodeType make_constant_int()
    {
        NodeType nt;
        nt.type     = "constant<int>";
        nt.library  = "math";
        nt.category = "constant";
        nt.help     = "Constant int source";
        nt.attributes = {
            { "value", "int", AttributeSpec::Role::Member, "0" },
            { "out",   "int", AttributeSpec::Role::Output, ""  },
        };
        return nt;
    }

    NodeType make_sin_wave()
    {
        NodeType nt;
        nt.type     = "sin_wave";
        nt.library  = "math";
        nt.category = "generator";
        nt.help     = "Sine wave generator";
        nt.attributes = {
            { "frequency", "float", AttributeSpec::Role::Member, "1.0" },
            { "amplitude", "float", AttributeSpec::Role::Member, "1.0" },
            { "phase",     "float", AttributeSpec::Role::Member, "0.0" },
            { "out",       "float", AttributeSpec::Role::Output, ""    },
        };
        return nt;
    }

    NodeType make_random()
    {
        NodeType nt;
        nt.type     = "random";
        nt.library  = "math";
        nt.category = "generator";
        nt.help     = "Uniform random float generator";
        nt.attributes = {
            { "seed", "int",   AttributeSpec::Role::Member, "0"   },
            { "min",  "float", AttributeSpec::Role::Member, "0.0" },
            { "max",  "float", AttributeSpec::Role::Member, "1.0" },
            { "out",  "float", AttributeSpec::Role::Output, ""    },
        };
        return nt;
    }

    NodeType make_add()
    {
        NodeType nt;
        nt.type     = "add";
        nt.library  = "math";
        nt.category = "arithmetic";
        nt.help     = "Sum of two floats";
        nt.attributes = {
            { "a",   "float", AttributeSpec::Role::Input,  "" },
            { "b",   "float", AttributeSpec::Role::Input,  "" },
            { "out", "float", AttributeSpec::Role::Output, "" },
        };
        return nt;
    }

    NodeType make_low_pass()
    {
        NodeType nt;
        nt.type     = "low_pass";
        nt.library  = "math";
        nt.category = "filter";
        nt.help     = "First-order low-pass filter";
        nt.attributes = {
            { "in",     "float", AttributeSpec::Role::Input,  ""     },
            { "cutoff", "float", AttributeSpec::Role::Member, "10.0" },
            { "out",    "float", AttributeSpec::Role::Output, ""     },
        };
        return nt;
    }

    NodeType make_cast_to_int()
    {
        NodeType nt;
        nt.type     = "cast<int>";
        nt.library  = "math";
        nt.category = "convert";
        nt.help     = "Truncates a float to an int";
        nt.attributes = {
            { "in",  "float", AttributeSpec::Role::Input,  "" },
            { "out", "int",   AttributeSpec::Role::Output, "" },
        };
        return nt;
    }

    NodeType make_cast_to_float()
    {
        NodeType nt;
        nt.type     = "cast<float>";
        nt.library  = "math";
        nt.category = "convert";
        nt.help     = "Promotes an int to a float";
        nt.attributes = {
            { "in",  "int",   AttributeSpec::Role::Input,  "" },
            { "out", "float", AttributeSpec::Role::Output, "" },
        };
        return nt;
    }

    NodeType make_probe_float()
    {
        NodeType nt;
        nt.type     = "probe<float>";
        nt.library  = "io";
        nt.category = "probe";
        nt.help     = "Inspection sink for a float signal";
        nt.attributes = {
            { "in", "float", AttributeSpec::Role::Input, "" },
        };
        return nt;
    }

    NodeType make_probe_int()
    {
        NodeType nt;
        nt.type     = "probe<int>";
        nt.library  = "io";
        nt.category = "probe";
        nt.help     = "Inspection sink for an int signal";
        nt.attributes = {
            { "in", "int", AttributeSpec::Role::Input, "" },
        };
        return nt;
    }

    // ---- example::* ----
    // Illustrative nodes used by the bundled walkthrough. Pin
    // semantics are deliberately generic ("command" / "measured")
    // because the kind of value flowing through (torque, position,
    // velocity, ...) is engine-defined. Replace these with your
    // real domain-specific types in production registries.

    NodeType make_jacobian_2x2()
    {
        NodeType nt;
        nt.type     = "jacobian_2x2";
        nt.library  = "example";
        nt.category = "example";
        nt.help     = "Example: 2x2 linear transform (e.g. cartesian -> joint). "
                      "Generic float pins; replace with your engine's kinematics node.";
        nt.attributes = {
            { "in_a",  "float", AttributeSpec::Role::Input,  ""    },
            { "in_b",  "float", AttributeSpec::Role::Input,  ""    },
            { "out_a", "float", AttributeSpec::Role::Output, ""    },
            { "out_b", "float", AttributeSpec::Role::Output, ""    },
            { "j00",   "float", AttributeSpec::Role::Member, "1.0" },
            { "j01",   "float", AttributeSpec::Role::Member, "0.0" },
            { "j10",   "float", AttributeSpec::Role::Member, "0.0" },
            { "j11",   "float", AttributeSpec::Role::Member, "1.0" },
        };
        return nt;
    }

    NodeType make_motor()
    {
        NodeType nt;
        nt.type     = "motor";
        nt.library  = "example";
        nt.category = "example";
        nt.help     = "Example: motor with one generic command input and one "
                      "measured-output. Pin semantics (torque / position / "
                      "velocity) are engine-defined; rename or split this node "
                      "in your registry.";
        nt.attributes = {
            { "command",  "float", AttributeSpec::Role::Input,  ""    },
            { "measured", "float", AttributeSpec::Role::Output, ""    },
            { "ratio",    "float", AttributeSpec::Role::Member, "1.0" },
        };
        return nt;
    }

    void register_builtin_nodes(NodeRegistry& reg)
    {
        reg.add(make_constant_float());
        reg.add(make_constant_int());
        reg.add(make_sin_wave());
        reg.add(make_random());
        reg.add(make_add());
        reg.add(make_low_pass());
        reg.add(make_cast_to_int());
        reg.add(make_cast_to_float());
        reg.add(make_probe_float());
        reg.add(make_probe_int());
        reg.add(make_jacobian_2x2());
        reg.add(make_motor());
    }
}
