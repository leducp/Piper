#ifndef PIPER_SERIALIZE_V2_H
#define PIPER_SERIALIZE_V2_H

#include <string>
#include <string_view>
#include <vector>

#include "piper/diagnostic.h"
#include "piper/graph.h"
#include "piper/registry.h"

namespace piper::v2
{
    constexpr int format_version = 2;

    // ---- Graph (designed pipeline) ----

    struct LoadResult
    {
        Graph                   graph;
        std::vector<Diagnostic> diagnostics;
    };

    // Always succeeds for any well-formed Graph.
    std::string serialize(Graph const& g);

    // Throws std::runtime_error only on malformed JSON or unsupported
    // version. All structural drift (unknown node types, orphan links,
    // unknown stage references, type mismatches) is reported via
    // LoadResult::diagnostics — the graph still loads with verbatim
    // data so the editor can surface and let the user fix.
    LoadResult deserialize(std::string_view json, NodeRegistry const& registry);

    // ---- Registry (engine's node-type catalog) ----

    struct RegistryLoadResult
    {
        NodeRegistry            registry;
        std::vector<Diagnostic> diagnostics;
    };

    // Always succeeds for any well-formed NodeRegistry. Type ordering
    // is implementation-defined — the registry stores types in a hash
    // map, so emit order doesn't match insertion order.
    std::string serialize_registry(NodeRegistry const& reg);

    // Same throw contract as deserialize: malformed JSON / wrong
    // version throws; everything else (duplicate type names, unknown
    // roles, missing fields) is reported via diagnostics.
    RegistryLoadResult deserialize_registry(std::string_view json);
}

#endif
