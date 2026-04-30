#ifndef PIPER_LINK_H
#define PIPER_LINK_H

#include <stdint.h>
#include <string>

#include "piper/node.h"

namespace piper
{
    using LinkId = uint64_t;
    constexpr LinkId invalid_link_id = 0;

    struct PinRef
    {
        NodeId      node{invalid_node_id};
        std::string attr;

        bool operator==(PinRef const& other) const
        {
            return node == other.node and attr == other.attr;
        }

        bool operator!=(PinRef const& other) const
        {
            return not (*this == other);
        }
    };

    struct Link
    {
        LinkId      id{invalid_link_id};
        PinRef      from;       // output pin
        PinRef      to;         // input pin
        std::string data_type;  // snapshot at link-creation time
    };
}

#endif
