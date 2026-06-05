#include <stdint.h>

#include <string>
#include <type_traits>

#include "piper/builtin_nodes.h"

#include "piper/node_type.h"
#include "piper/vec.h"

namespace piper
{
    // Map a C++ type to its canonical pin data_type string used in
    // editor metadata and JSON. Add a branch when introducing a new T.
    template<typename T>
    constexpr char const* data_type_string()
    {
        if      constexpr (std::is_same_v<T, float>)         { return "float";       }
        else if constexpr (std::is_same_v<T, double>)        { return "double";      }
        else if constexpr (std::is_same_v<T, int8_t>)        { return "int8_t";      }
        else if constexpr (std::is_same_v<T, int16_t>)       { return "int16_t";     }
        else if constexpr (std::is_same_v<T, int32_t>)       { return "int32_t";     }
        else if constexpr (std::is_same_v<T, int64_t>)       { return "int64_t";     }
        else if constexpr (std::is_same_v<T, uint8_t>)       { return "uint8_t";     }
        else if constexpr (std::is_same_v<T, uint16_t>)      { return "uint16_t";    }
        else if constexpr (std::is_same_v<T, uint32_t>)      { return "uint32_t";    }
        else if constexpr (std::is_same_v<T, uint64_t>)      { return "uint64_t";    }
        else if constexpr (std::is_same_v<T, Vec2<float>>)   { return "vec2<float>"; }
        else if constexpr (std::is_same_v<T, Vec3<float>>)   { return "vec3<float>"; }
        else
        {
            static_assert(sizeof(T) == 0, "data_type_string<T>: unsupported T");
        }
    }

    template<typename T>
    constexpr char const* default_value_string()
    {
        if      constexpr (std::is_same_v<T, float>)         { return "0.0";   }
        else if constexpr (std::is_same_v<T, double>)        { return "0.0";   }
        else if constexpr (std::is_integral_v<T>)            { return "0";     }
        else if constexpr (std::is_same_v<T, Vec2<float>>)   { return "0,0";   }
        else if constexpr (std::is_same_v<T, Vec3<float>>)   { return "0,0,0"; }
        else
        {
            static_assert(sizeof(T) == 0, "default_value_string<T>: unsupported T");
        }
    }

    template<typename T>
    std::string typed_node_name(std::string const& base)
    {
        return base + "<" + data_type_string<T>() + ">";
    }

    template<typename T>
    NodeType make_constant()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("constant");
        nt.category = "constant";
        nt.help     = std::string("Constant ") + data_type_string<T>() + " source";
        nt.attributes = {
            { "value", data_type_string<T>(), AttributeSpec::Role::Member, default_value_string<T>() },
            { "out",   data_type_string<T>(), AttributeSpec::Role::Output, ""                        },
        };
        return nt;
    }

    template<typename T>
    NodeType make_sin_wave()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("sin_wave");
        nt.category = "generator";
        nt.help     = std::string("Sine wave generator (") + data_type_string<T>() + " output). "
                      "dt is the per-tick time step the host advances; "
                      "set it to whatever cadence your loop runs at.";
        nt.attributes = {
            { "frequency", "float",                AttributeSpec::Role::Member, "1.0"   },
            { "amplitude", "float",                AttributeSpec::Role::Member, "1.0"   },
            { "phase",     "float",                AttributeSpec::Role::Member, "0.0"   },
            { "dt",        "float",                AttributeSpec::Role::Member, "0.001" },
            { "dt_in",     data_type_string<T>(),  AttributeSpec::Role::Input,  "", false, true },
            { "out",       data_type_string<T>(),  AttributeSpec::Role::Output, ""      },
        };
        return nt;
    }

    template<typename T>
    NodeType make_low_pass()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("low_pass");
        nt.category = "filter";
        nt.help     = std::string("First-order low-pass filter (") + data_type_string<T>() + "). "
                      "dt is the per-tick time step the host advances.";
        nt.attributes = {
            { "in",     data_type_string<T>(), AttributeSpec::Role::Input,  ""      },
            { "cutoff", "float",               AttributeSpec::Role::Member, "10.0"  },
            { "dt",     "float",               AttributeSpec::Role::Member, "0.001" },
            { "dt_in",  data_type_string<T>(), AttributeSpec::Role::Input,  "", false, true },
            { "out",    data_type_string<T>(), AttributeSpec::Role::Output, ""      },
        };
        return nt;
    }

    template<typename T>
    NodeType make_probe()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("probe");
        nt.category = "probe";
        nt.help     = std::string("Inspection sink for a ") + data_type_string<T>()
                    + " signal (no engine impl).";
        nt.attributes = {
            { "in", data_type_string<T>(), AttributeSpec::Role::Input, "" },
        };
        return nt;
    }

    template<typename T>
    NodeType make_external_input()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("external_input");
        nt.category = "external";
        nt.help     = std::string("Externally-set ") + data_type_string<T>()
                    + " source. Use Engine::input<" + data_type_string<T>()
                    + ">(name). min/max bound the slider in the editor's "
                      "Live panel; the engine itself does no clamping.";
        char const* const min_default = std::is_floating_point_v<T> ? "-1.0" : "-100";
        char const* const max_default = std::is_floating_point_v<T> ? "1.0"  : "100";
        nt.attributes = {
            { "name", "string",              AttributeSpec::Role::Member, ""          },
            { "min",  data_type_string<T>(), AttributeSpec::Role::Member, min_default },
            { "max",  data_type_string<T>(), AttributeSpec::Role::Member, max_default },
            { "out",  data_type_string<T>(), AttributeSpec::Role::Output, ""          },
        };
        return nt;
    }

    template<typename T>
    NodeType make_external_output()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("external_output");
        nt.category = "external";
        nt.help     = std::string("Externally-read ") + data_type_string<T>()
                    + " sink. Use Engine::output<" + data_type_string<T>() + ">(name).";
        nt.attributes = {
            { "name", "string",              AttributeSpec::Role::Member, "" },
            { "in",   data_type_string<T>(), AttributeSpec::Role::Input,  "" },
        };
        return nt;
    }

    NodeType make_random()
    {
        NodeType nt;
        nt.type     = "random";
        nt.category = "generator";
        nt.help     = "Uniform random float generator";
        nt.attributes = {
            { "seed", "int32_t",   AttributeSpec::Role::Member, "0"   },
            { "min",  "float", AttributeSpec::Role::Member, "0.0" },
            { "max",  "float", AttributeSpec::Role::Member, "1.0" },
            { "out",  "float", AttributeSpec::Role::Output, ""    },
        };
        return nt;
    }

    template<typename T>
    NodeType make_add()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("add");
        nt.category = "arithmetic";
        nt.help     = std::string("Sum of two ") + data_type_string<T>() + " values";
        nt.attributes = {
            { "a",   data_type_string<T>(), AttributeSpec::Role::Input,  "" },
            { "b",   data_type_string<T>(), AttributeSpec::Role::Input,  "" },
            { "out", data_type_string<T>(), AttributeSpec::Role::Output, "" },
        };
        return nt;
    }

    template<typename T>
    NodeType make_subtract()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("subtract");
        nt.category = "arithmetic";
        nt.help     = std::string("Difference of two ") + data_type_string<T>() + " values (a - b)";
        nt.attributes = {
            { "a",   data_type_string<T>(), AttributeSpec::Role::Input,  "" },
            { "b",   data_type_string<T>(), AttributeSpec::Role::Input,  "" },
            { "out", data_type_string<T>(), AttributeSpec::Role::Output, "" },
        };
        return nt;
    }

    template<typename T>
    NodeType make_mux3()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("mux3");
        nt.category = "control";
        nt.help     = std::string("3-input ") + data_type_string<T>()
                    + " multiplexer; int32_t selector saturates at 0 and 2";
        nt.attributes = {
            { "in0",    data_type_string<T>(), AttributeSpec::Role::Input,  "" },
            { "in1",    data_type_string<T>(), AttributeSpec::Role::Input,  "" },
            { "in2",    data_type_string<T>(), AttributeSpec::Role::Input,  "" },
            { "select", "int32_t",             AttributeSpec::Role::Input,  "" },
            { "out",    data_type_string<T>(), AttributeSpec::Role::Output, "" },
        };
        return nt;
    }

    template<typename T>
    NodeType make_clamp()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("clamp");
        nt.category = "control";
        nt.help     = std::string("Saturates a ") + data_type_string<T>()
                    + " to [min, max].";
        nt.attributes = {
            { "in",  data_type_string<T>(), AttributeSpec::Role::Input,  ""     },
            { "min", data_type_string<T>(), AttributeSpec::Role::Member, "-1.0" },
            { "max", data_type_string<T>(), AttributeSpec::Role::Member, "1.0"  },
            { "out", data_type_string<T>(), AttributeSpec::Role::Output, ""     },
        };
        return nt;
    }

    template<typename T>
    NodeType make_pid()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("pid");
        nt.category = "control";
        nt.help     = std::string("Discrete PID (") + data_type_string<T>()
                    + "); kp/ki/kd as inputs so they can be wired to constants, "
                      "presets, or live signals. dt is the per-tick time step.";
        nt.attributes = {
            { "setpoint", data_type_string<T>(), AttributeSpec::Role::Input,  ""      },
            { "measured", data_type_string<T>(), AttributeSpec::Role::Input,  ""      },
            { "kp",       data_type_string<T>(), AttributeSpec::Role::Input,  ""      },
            { "ki",       data_type_string<T>(), AttributeSpec::Role::Input,  ""      },
            { "kd",       data_type_string<T>(), AttributeSpec::Role::Input,  ""      },
            { "dt",       "float",               AttributeSpec::Role::Member, "0.001" },
            { "dt_in",    data_type_string<T>(), AttributeSpec::Role::Input,  "", false, true },
            // Output saturation + conditional integration. Defaults are
            // ~unbounded so behavior is unchanged unless the user sets
            // a real actuator limit.
            { "out_min",  "float",               AttributeSpec::Role::Member, "-1e30" },
            { "out_max",  "float",               AttributeSpec::Role::Member, "1e30"  },
            { "out",      data_type_string<T>(), AttributeSpec::Role::Output, ""      },
        };
        return nt;
    }

    template<typename T>
    NodeType make_preset3()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("preset3");
        nt.category = "control";
        nt.help     = std::string("3-slot mode-keyed ") + data_type_string<T>()
                    + " bank: publishes the slot whose label matches the "
                      "node's per-profile label; T{} when no slot matches";
        nt.attributes = {
            { "label0", "string",              AttributeSpec::Role::Member, "case0", true },
            { "value0", data_type_string<T>(), AttributeSpec::Role::Member, default_value_string<T>() },
            { "label1", "string",              AttributeSpec::Role::Member, "case1", true },
            { "value1", data_type_string<T>(), AttributeSpec::Role::Member, default_value_string<T>() },
            { "label2", "string",              AttributeSpec::Role::Member, "case2", true },
            { "value2", data_type_string<T>(), AttributeSpec::Role::Member, default_value_string<T>() },
            { "out",    data_type_string<T>(), AttributeSpec::Role::Output, "" },
        };
        return nt;
    }

    template<typename T>
    NodeType make_multiply()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("multiply");
        nt.category = "arithmetic";
        nt.help     = std::string("Product of two ") + data_type_string<T>() + " values";
        nt.attributes = {
            { "a",   data_type_string<T>(), AttributeSpec::Role::Input,  "" },
            { "b",   data_type_string<T>(), AttributeSpec::Role::Input,  "" },
            { "out", data_type_string<T>(), AttributeSpec::Role::Output, "" },
        };
        return nt;
    }

    template<typename T>
    NodeType make_abs()
    {
        NodeType nt;
        nt.type     = typed_node_name<T>("abs");
        nt.category = "arithmetic";
        nt.help     = std::string("Absolute value of a ") + data_type_string<T>();
        nt.attributes = {
            { "in",  data_type_string<T>(), AttributeSpec::Role::Input,  "" },
            { "out", data_type_string<T>(), AttributeSpec::Role::Output, "" },
        };
        return nt;
    }

    NodeType make_cast_to_int()
    {
        NodeType nt;
        nt.type     = "cast<int32_t>";
        nt.category = "convert";
        nt.help     = "Truncates a float to an int";
        nt.attributes = {
            { "in",  "float", AttributeSpec::Role::Input,  "" },
            { "out", "int32_t",   AttributeSpec::Role::Output, "" },
        };
        return nt;
    }

    NodeType make_cast_to_float()
    {
        NodeType nt;
        nt.type     = "cast<float>";
        nt.category = "convert";
        nt.help     = "Promotes an int to a float";
        nt.attributes = {
            { "in",  "int32_t",   AttributeSpec::Role::Input,  "" },
            { "out", "float", AttributeSpec::Role::Output, "" },
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
        // ---- math ----
        reg.add("math", make_constant<float>());
        reg.add("math", make_constant<int32_t>());
        reg.add("math", make_constant<Vec2<float>>());
        reg.add("math", make_constant<Vec3<float>>());
        reg.add("math", make_sin_wave<float>());
        reg.add("math", make_sin_wave<double>());
        reg.add("math", make_random());
        reg.add("math", make_add<float>());
        reg.add("math", make_add<double>());
        reg.add("math", make_add<int32_t>());
        reg.add("math", make_subtract<float>());
        reg.add("math", make_subtract<double>());
        reg.add("math", make_subtract<int32_t>());
        reg.add("math", make_add<Vec2<float>>());
        reg.add("math", make_add<Vec3<float>>());
        reg.add("math", make_subtract<Vec2<float>>());
        reg.add("math", make_subtract<Vec3<float>>());
        reg.add("math", make_multiply<float>());
        reg.add("math", make_multiply<double>());
        reg.add("math", make_multiply<int32_t>());
        reg.add("math", make_abs<float>());
        reg.add("math", make_abs<double>());
        reg.add("math", make_abs<int32_t>());
        reg.add("math", make_low_pass<float>());
        reg.add("math", make_low_pass<double>());
        reg.add("math", make_cast_to_int());
        reg.add("math", make_cast_to_float());

        reg.add("control", make_mux3<float>());
        reg.add("control", make_mux3<double>());
        reg.add("control", make_mux3<int32_t>());
        reg.add("control", make_clamp<float>());
        reg.add("control", make_clamp<double>());
        reg.add("control", make_clamp<int32_t>());
        reg.add("control", make_pid<float>());
        reg.add("control", make_pid<double>());
        reg.add("control", make_preset3<float>());
        reg.add("control", make_preset3<double>());
        reg.add("control", make_preset3<int32_t>());

        // ---- io ----
        reg.add("io", make_external_input<float>());
        reg.add("io", make_external_input<double>());
        reg.add("io", make_external_input<int32_t>());
        reg.add("io", make_external_output<float>());
        reg.add("io", make_external_output<double>());
        reg.add("io", make_external_output<int32_t>());

        // ---- example: nodes with no engine impl ----
        reg.add("example", make_probe<float>());
        reg.add("example", make_probe<int32_t>());
        reg.add("example", make_jacobian_2x2());
        reg.add("example", make_motor());
    }
}
