#ifndef PIPER_NODE_TYPE_H
#define PIPER_NODE_TYPE_H

#include <string>
#include <vector>

#include "piper/attribute.h"

namespace piper
{
    struct NodeType
    {
        std::string type;       // unique key in the registry
        std::string help;
        std::string category;
        std::vector<AttributeSpec> attributes;
        // Modes the step handles itself; copied into each new node, where the
        // user may add more. "disable" is implicit; none declared means a new
        // node starts with "enable".
        std::vector<std::string> modes;
    };
}

#endif
