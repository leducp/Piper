#ifndef PIPER_ATTRIBUTE_H
#define PIPER_ATTRIBUTE_H

#include <map>
#include <string>
#include <vector>

namespace piper
{
    // Lives in the registry; never mutated after registration.
    struct AttributeSpec
    {
        enum class Role
        {
            Input,
            Output,
            Member,
            ExternalInput,
            ExternalOutput,
        };

        std::string name;
        std::string data_type;
        Role        role{Role::Member};
        std::string default_value;
        // Input-role only: allows an unwired pin and dims it on the canvas.
        // The step must handle missing input, e.g. by using a member default.
        bool        is_optional{false};
    };

    constexpr bool is_pin(AttributeSpec::Role r)
    {
        return r == AttributeSpec::Role::Input or r == AttributeSpec::Role::Output;
    }

    constexpr bool is_external(AttributeSpec::Role r)
    {
        return r == AttributeSpec::Role::ExternalInput or r == AttributeSpec::Role::ExternalOutput;
    }

    // Saved name/data_type/role are compared with the registry at load
    // time to detect schema drift.
    struct Attribute
    {
        // name is the stable handle for PinRef::attr; never mutated after
        // Graph::add_node synthesizes the attribute from the spec.
        std::string         name;
        std::string         data_type;
        AttributeSpec::Role role{AttributeSpec::Role::Member};
        // Member default value; mode_overloads replaces it in the listed modes.
        std::string         value;
        std::map<std::string, std::string> mode_overloads;
        // Optional per-pin stage override. Empty = inherit Node::stage.
        std::vector<std::string> stages;
        // Renders the pin on the side opposite its role (input on the
        // right, output on the left). Purely visual -- helps route
        // feedback/cycle connections cleanly. Default false.
        bool flip_side{false};
    };
}

#endif
