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
        // Adds the type with no library tag.
        bool add(NodeType const& type);

        // Adds the type and records its library. Library is a
        // registration-time sorting concern -- the editor uses it to
        // group nodes in the palette; runtime users (engines) ignore
        // it. NodeType itself does not carry a library field.
        bool add(std::string library, NodeType const& type);

        NodeType const* find(std::string_view type_name) const;

        // Returns the library tag a type was registered under, or an
        // empty string if no library was specified at registration or
        // the type is unknown.
        std::string_view library_of(std::string_view type_name) const;

        // Pointers are valid only until the next add() call.
        std::vector<NodeType const*> all() const;

        std::size_t size() const { return types_.size(); }
        bool empty() const       { return types_.empty(); }

    private:
        std::unordered_map<std::string, NodeType>   types_;
        std::unordered_map<std::string, std::string> library_of_;
    };
}

#endif
