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

        // Computation phases this step type exposes. The user binds
        // each entry to a graph stage in the editor (per node). Empty
        // is treated as a single implicit "tick" slot to keep simple
        // single-phase steps zero-config.
        std::vector<std::string> slots;
    };
}

#endif
