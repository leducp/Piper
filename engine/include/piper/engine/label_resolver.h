#ifndef PIPER_ENGINE_LABEL_RESOLVER_H
#define PIPER_ENGINE_LABEL_RESOLVER_H

#include <vector>

#include "piper/engine/diagnostic.h"
#include "piper/graph.h"
#include "piper/link.h"

namespace piper::engine
{
    // Lowers each label cluster into synthetic Links that bypass the
    // labels and connect their upstream producer pin straight to each
    // downstream consumer pin. Returns the union of:
    //   1. graph.links() minus any link touching a label, and
    //   2. one synthetic Link per (label_in -> label_out -> consumer)
    //      cluster path.
    // Synthetic links carry `invalid_link_id` so callers can tell them
    // from real links. Cluster validation diagnostics (no source,
    // multiple sources, no consumer, chained labels) are appended to
    // `diags`; type checking is left to the caller's wire-pass.
    std::vector<piper::Link>
    resolve_label_clusters(piper::Graph const&            graph,
                           std::vector<BuildDiagnostic>&  diags,
                           bool&                          has_error);
}

#endif
