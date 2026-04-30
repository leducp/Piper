#ifndef PIPER_NODE_H
#define PIPER_NODE_H

#include <stdint.h>

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "piper/attribute.h"

namespace piper
{
    using NodeId = uint64_t;
    constexpr NodeId invalid_node_id = 0;

    struct Point
    {
        float x{0.0f};
        float y{0.0f};

        constexpr bool operator==(Point const& other) const
        {
            return x == other.x and y == other.y;
        }

        constexpr bool operator!=(Point const& other) const
        {
            return not (*this == other);
        }
    };

    struct Node
    {
        NodeId      id{invalid_node_id};
        std::string type;
        std::string name;
        Point       pos;
        std::vector<Attribute> attrs;

        // Per-instance slot-to-stage binding. Each key is a slot name
        // declared on the node's NodeType; each value is a stage name
        // declared on the graph. A slot with no entry here does not
        // tick. Empty bindings means the node never ticks.
        std::map<std::string, std::string> slot_bindings;

        Attribute const* find_attr(std::string_view attr_name) const
        {
            for (auto const& a : attrs)
            {
                if (a.name == attr_name)
                {
                    return &a;
                }
            }
            return nullptr;
        }
    };
}

#endif
