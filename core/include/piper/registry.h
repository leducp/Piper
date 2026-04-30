#ifndef PIPER_REGISTRY_H
#define PIPER_REGISTRY_H

#include <string_view>
#include <unordered_map>
#include <vector>

#include "piper/node_type.h"

namespace piper
{
    class NodeRegistry
    {
    public:
        // Duplicate type name: existing entry kept, returns false.
        bool add(NodeType const& type);

        // Convenience: stamps `type.library = library` before adding.
        // Library is a registration-time sorting concern, so factories
        // that build NodeType need not bake it into their definition.
        bool add(std::string library, NodeType type);

        NodeType const* find(std::string_view type_name) const;

        // Pointers are valid only until the next add() call.
        std::vector<NodeType const*> all() const;

        std::size_t size() const { return types_.size(); }
        bool empty() const       { return types_.empty(); }

    private:
        std::unordered_map<std::string, NodeType> types_;
    };
}

#endif
