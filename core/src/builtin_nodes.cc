#include "piper/builtin_nodes.h"

#include "piper/node_type.h"

namespace piper
{
    NodeType make_sin_wave()
    {
        NodeType nt;
        nt.type     = "SinWave";
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
        nt.type     = "Random";
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
        nt.type     = "Add";
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
        nt.type     = "LowPass";
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

    NodeType make_cast_float_int()
    {
        NodeType nt;
        nt.type     = "CastFloatInt";
        nt.library  = "math";
        nt.category = "convert";
        nt.help     = "Truncates a float to an int";
        nt.attributes = {
            { "in",  "float", AttributeSpec::Role::Input,  "" },
            { "out", "int",   AttributeSpec::Role::Output, "" },
        };
        return nt;
    }

    NodeType make_cast_int_float()
    {
        NodeType nt;
        nt.type     = "CastIntFloat";
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
        nt.type     = "ProbeFloat";
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
        nt.type     = "ProbeInt";
        nt.library  = "io";
        nt.category = "probe";
        nt.help     = "Inspection sink for an int signal";
        nt.attributes = {
            { "in", "int", AttributeSpec::Role::Input, "" },
        };
        return nt;
    }

    void register_builtin_nodes(NodeRegistry& reg)
    {
        reg.add(make_sin_wave());
        reg.add(make_random());
        reg.add(make_add());
        reg.add(make_low_pass());
        reg.add(make_cast_float_int());
        reg.add(make_cast_int_float());
        reg.add(make_probe_float());
        reg.add(make_probe_int());
    }
}
