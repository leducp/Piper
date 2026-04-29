#ifndef PIPER_MIGRATE_V1_READER_H
#define PIPER_MIGRATE_V1_READER_H

#include <string_view>

#include "piper/diagnostic.h"
#include "piper/graph.h"
#include "piper/registry.h"

namespace piper::migrate
{
    struct Options
    {
        // --strict upgrades reader warnings (orphan links, unknown
        // mode labels, ...) into hard errors. Implemented in PR 3.4;
        // honored here as a no-op until then.
        bool strict = false;
    };

    struct LoadResult
    {
        Graph                   graph;
        std::vector<Diagnostic> diagnostics;
    };

    // Reads a V1-era pipeline JSON document and produces a piper::Graph
    // populated against `registry`. Throws std::runtime_error on
    // malformed JSON or when --strict is set and any warning fires.
    // Structural drift (orphan links, unknown attributes, ...) is
    // reported via LoadResult::diagnostics in non-strict mode.
    LoadResult read_v1(std::string_view             json,
                       NodeRegistry const&          registry,
                       Options const&               opts = {});
}

#endif
