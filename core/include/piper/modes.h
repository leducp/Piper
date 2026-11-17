#ifndef PIPER_MODES_H
#define PIPER_MODES_H

#include <string>
#include <string_view>
#include <vector>

#include "piper/diagnostic.h"
#include "piper/graph.h"
#include "piper/node.h"
#include "piper/profile.h"
#include "piper/registry.h"

namespace piper
{
    // Modes a profile may assign to `node`, without duplicates: the type's
    // modes, then the node's own; "enable" when both are empty; "disable"
    // always comes last. The first entry is the default mode.
    std::vector<std::string> modes_of(Node const& node, NodeType const* type);
    std::vector<std::string> modes_of(Node const& node, NodeRegistry const& reg);

    // False for an empty mode; true for any non-empty mode of an unknown type.
    bool accepts_mode(Node const& node, NodeRegistry const& reg, std::string_view mode);

    // Gives every graph node without an entry in `profile` its first mode.
    void fill_missing_modes(Profile& profile, Graph const& graph, NodeRegistry const& reg);

    // MissingMode for each node without a mode in a profile, UnknownMode
    // for each mode that accepts_mode() rejects.
    std::vector<Diagnostic> check_profile_modes(Graph const& graph, NodeRegistry const& reg);
}

#endif
