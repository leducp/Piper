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
        };

        std::string name;
        std::string data_type;
        Role        role{Role::Member};
        std::string default_value;
        // True for string members whose value is a mode-profile label
        // this node's step dispatches on (e.g. preset3's label0/label1/
        // label2). The editor's mode-label picker reads them off the
        // node's instance attrs to show a node-specific menu, so the
        // user never types "tight" / "bypass" twice.
        bool        is_mode_label{false};
        // Input-role only: when true, the linter does not flag the pin
        // as missing a source, and the canvas dims it so users can see
        // at a glance which inputs are optional. The step's compute()
        // is expected to fall back to a member or sensible default if
        // the pin is unwired (e.g. dt_in on sin_wave).
        bool        is_optional{false};
    };

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
    };
}

#endif
