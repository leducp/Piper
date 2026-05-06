#ifndef PIPER_REGISTRY_H
#define PIPER_REGISTRY_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "piper/node.h"
#include "piper/node_type.h"

namespace piper
{
    class NodeRegistry;

    // Returns the live values of `node`'s string attrs whose spec is
    // marked `is_mode_label` -- the per-instance set of mode labels
    // this node's step dispatches on. Empty when the type is unknown,
    // has no such specs, or the node lacks the corresponding attrs.
    // Callers (mainly the editor's mode-label picker) use this to
    // surface node-specific labels without the user having to retype
    // them as free strings.
    std::vector<std::string> mode_labels_advertised_by(
        Node const& node, NodeRegistry const& reg);

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
