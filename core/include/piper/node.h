#ifndef PIPER_NODE_H
#define PIPER_NODE_H

#include <cstdint>
#include <string>
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
        std::string stage;
        Point       pos;
        std::vector<Attribute> attrs;
    };
}

#endif
