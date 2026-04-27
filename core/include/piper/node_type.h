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
        std::string library;
        std::string category;
        std::vector<AttributeSpec> attributes;
    };
}

#endif
