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
        // Input-role only: when true, the linter does not flag the pin
        // as missing a source, and the canvas dims it so users can see
        // at a glance which inputs are optional. The step's compute()
        // is expected to fall back to a member or sensible default if
        // the pin is unwired (e.g. dt_in on sin_wave).
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

    // DELIBERATELY duplicates name/data_type/role from AttributeSpec.
    // Mismatch with the current registry is the load-time drift signal.
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
