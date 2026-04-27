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
