#include <stdint.h>

#include <string>
#include <type_traits>

#include "piper/builtin_nodes.h"

#include "piper/builtin_types.h"
#include "piper/node_type.h"
#include "piper/vec.h"

namespace piper
{
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

    // Default bounds for the min/max member pair on clamp and
    // external_input. An unsigned T must not default to a negative
    // string: the step's member parser saturates it to 0, which would
    // silently disagree with the number shown in the inspector.
    template<typename T>
    constexpr char const* range_min_string()
    {
        if      constexpr (std::is_floating_point_v<T>) { return "-1.0"; }
        else if constexpr (std::is_signed_v<T>)         { return "-100"; }
        else                                            { return "0";    }
    }

    template<typename T>
    constexpr char const* range_max_string()
    {
        if constexpr (std::is_floating_point_v<T>) { return "1.0"; }
        else                                       { return "100"; }
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
        nt.attributes = {
            { "name", "string",              AttributeSpec::Role::Member, ""                    },
            { "min",  data_type_string<T>(), AttributeSpec::Role::Member, range_min_string<T>() },
            { "max",  data_type_string<T>(), AttributeSpec::Role::Member, range_max_string<T>() },
            { "out",  data_type_string<T>(), AttributeSpec::Role::Output, ""                    },
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
            { "in",  data_type_string<T>(), AttributeSpec::Role::Input,  ""                   },
            { "min", data_type_string<T>(), AttributeSpec::Role::Member, range_min_string<T>() },
            { "max", data_type_string<T>(), AttributeSpec::Role::Member, range_max_string<T>() },
            { "out", data_type_string<T>(), AttributeSpec::Role::Output, ""                   },
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

    // Named by both ends: the destination alone does not identify the
    // conversion once more than two scalar types exist. Submenu is
    // keyed on the source so the convert tree stays navigable at
    // N*(N-1) entries.
    template<typename From, typename To>
    NodeType make_cast()
    {
        NodeType nt;
        nt.type     = std::string("cast<") + data_type_string<From>()
                    + "," + data_type_string<To>() + ">";
        nt.category = std::string("convert/from ") + data_type_string<From>();
        nt.help     = std::string("Converts a ") + data_type_string<From>()
                    + " to a " + data_type_string<To>()
                    + ". static_cast semantics: truncation toward zero on "
                      "float -> integer, modular wraparound on unsigned "
                      "overflow, implementation-defined on signed overflow.";
        nt.attributes = {
            { "in",  data_type_string<From>(), AttributeSpec::Role::Input,  "" },
            { "out", data_type_string<To>(),   AttributeSpec::Role::Output, "" },
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

    // Every node family that is meaningful for one scalar T. abs is
    // skipped for unsigned T (it would be the identity); the
    // time-stepped families are float-domain only.
    template<typename T>
    void register_scalar_nodes(NodeRegistry& reg)
    {
        reg.add("math", make_constant<T>());
        reg.add("math", make_add<T>());
        reg.add("math", make_subtract<T>());
        reg.add("math", make_multiply<T>());
        if constexpr (std::is_signed_v<T>)
        {
            reg.add("math", make_abs<T>());
        }
        if constexpr (std::is_floating_point_v<T>)
        {
            reg.add("math",    make_sin_wave<T>());
            reg.add("math",    make_low_pass<T>());
            reg.add("control", make_pid<T>());
        }

        reg.add("control", make_mux3<T>());
        reg.add("control", make_clamp<T>());
        reg.add("control", make_preset3<T>());

        reg.add("io", make_external_input<T>());
        reg.add("io", make_external_output<T>());

        reg.add("example", make_probe<T>());
    }

    template<typename... Ts>
    void register_scalar_nodes_for(NodeRegistry& reg, TypeList<Ts...>)
    {
        (register_scalar_nodes<Ts>(reg), ...);
    }

    template<typename T>
    void register_vector_nodes(NodeRegistry& reg)
    {
        reg.add("math", make_constant<T>());
        reg.add("math", make_add<T>());
        reg.add("math", make_subtract<T>());
    }

    template<typename... Ts>
    void register_vector_nodes_for(NodeRegistry& reg, TypeList<Ts...>)
    {
        (register_vector_nodes<Ts>(reg), ...);
    }

    template<typename From, typename To>
    void register_cast(NodeRegistry& reg)
    {
        if constexpr (not std::is_same_v<From, To>)
        {
            reg.add("math", make_cast<From, To>());
        }
    }

    template<typename From, typename... Ts>
    void register_casts_from(NodeRegistry& reg, TypeList<Ts...>)
    {
        (register_cast<From, Ts>(reg), ...);
    }

    template<typename... Ts>
    void register_casts_for(NodeRegistry& reg, TypeList<Ts...> list)
    {
        (register_casts_from<Ts>(reg, list), ...);
    }

    void register_builtin_nodes(NodeRegistry& reg)
    {
        register_scalar_nodes_for(reg, BuiltinScalars{});
        register_vector_nodes_for(reg, BuiltinVectors{});
        register_casts_for(reg, BuiltinScalars{});

        reg.add("math", make_random());

        // ---- example: nodes with no engine impl ----
        reg.add("example", make_jacobian_2x2());
        reg.add("example", make_motor());
    }
}
