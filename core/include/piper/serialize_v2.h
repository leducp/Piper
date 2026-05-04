#ifndef PIPER_SERIALIZE_V2_H
#define PIPER_SERIALIZE_V2_H

#include <string>
#include <string_view>
#include <vector>

#include "piper/diagnostic.h"
#include "piper/graph.h"
#include "piper/node.h"
#include "piper/node_type.h"
#include "piper/registry.h"

namespace piper::v2
{
    constexpr int format_version = 3;
    constexpr int min_supported_version = 2;

    // ---- Graph bundle (one or many pipelines) ----

    // Files always wrap their pipelines in a top-level array, even when
    // there's only one. A single-pipeline file is just a bundle of one.
    struct Pipeline
    {
        std::string             name;       // may be empty
        Graph                   graph;
        std::vector<Diagnostic> diagnostics;
    };

    struct BundleLoadResult
    {
        std::vector<Pipeline>   pipelines;
        std::vector<Diagnostic> diagnostics; // top-level (version, schema)
    };

    // Convenience type for the common single-pipeline case. Equivalent
    // to `BundleLoadResult` with exactly one pipeline.
    struct LoadResult
    {
        Graph                   graph;
        std::vector<Diagnostic> diagnostics;
    };

    struct PipelineRef
    {
        std::string  name;
        Graph const* graph;
    };

    // Writes a bundle. Produces JSON with a top-level "pipelines" array.
    std::string serialize_bundle(std::vector<PipelineRef> const& pipelines);

    // Convenience: writes a one-entry bundle.
    std::string serialize(Graph const& g, std::string const& name = {});

    // Throws std::runtime_error only on malformed JSON or unsupported
    // version. All structural drift is reported via per-pipeline
    // diagnostics (or top-level diagnostics for bundle-shape errors).
    BundleLoadResult deserialize_bundle(std::string_view json,
                                         NodeRegistry const& registry);

    // Convenience: returns the first pipeline as a LoadResult. Top-level
    // bundle diagnostics are merged into the returned LoadResult so they
    // surface to single-pipeline callers. If the bundle is empty (no
    // pipelines), returns an empty graph with the bundle diagnostics.
    LoadResult deserialize(std::string_view json, NodeRegistry const& registry);

    // ---- Registry (engine's node-type catalog) ----

    struct RegistryLoadResult
    {
        NodeRegistry            registry;
        std::vector<Diagnostic> diagnostics;
    };

    // Always succeeds for any well-formed NodeRegistry. Type ordering
    // is implementation-defined -- the registry stores types in a hash
    // map, so emit order doesn't match insertion order.
    std::string serialize_registry(NodeRegistry const& reg);

    // Same throw contract as deserialize: malformed JSON / wrong
    // version throws; everything else (duplicate type names, unknown
    // roles, missing fields) is reported via diagnostics.
    RegistryLoadResult deserialize_registry(std::string_view json);

    // Walks `node` against `spec`: saved-attr-not-in-spec ->
    // AttributeMissing, spec-attr-not-saved -> AttributeAdded, type
    // mismatch -> AttributeDrift. Append-only: never clears `diags`.
    // Re-runnable post-load when the registry gains new types.
    void check_attribute_drift(Node const&              node,
                                NodeType const&          spec,
                                std::vector<Diagnostic>& diags);
}

#endif
