#ifndef PIPER_ATTRIBUTE_H
#define PIPER_ATTRIBUTE_H

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
        // Marks string members that name modes handled by this step.
        // The editor offers their instance values in the mode picker.
        bool        is_mode_name{false};
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
        std::string         value;
        // Optional per-pin stage override. Empty = inherit Node::stage.
        std::vector<std::string> stages;
        // Renders the pin on the side opposite its role (input on the
        // right, output on the left). Purely visual -- helps route
        // feedback/cycle connections cleanly. Default false.
        bool flip_side{false};
    };
}

#endif
