#ifndef PIPER_MIGRATE_V1_READER_H
#define PIPER_MIGRATE_V1_READER_H

#include <string_view>

#include "piper/registry.h"
#include "piper/serialize_v2.h"

namespace piper::migrate
{
    struct Options
    {
    };

    // Reads a V1-era pipeline JSON document and produces a bundle of
    // V2 pipelines (one per top-level key in the V1 document). Returns
    // structural drift (orphan links, unknown attributes, ...) through
    // the per-pipeline diagnostics. Throws std::runtime_error only on
    // malformed JSON, input that is already a v2/v3 document, or
    // schema-level violations the reader cannot recover from.
    v2::BundleLoadResult read_v1(std::string_view             json,
                                  NodeRegistry const&          registry,
                                  Options const&               opts = {});
}

#endif
