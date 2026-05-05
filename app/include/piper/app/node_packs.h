#ifndef PIPER_APP_NODE_PACKS_H
#define PIPER_APP_NODE_PACKS_H

#include <string>
#include <vector>

namespace piper { class NodeRegistry; }

namespace piper::studio
{
    // $XDG_CONFIG_HOME/piper/nodes (fallback $HOME/.config/piper/nodes).
    // Empty if neither env var is set. Mirrors settings_path().
    std::string nodes_dir();

    struct NodePackLoadResult
    {
        int                      added{0};
        // Type names already present in the registry; existing entry kept.
        int                      skipped{0};
        // File-level failures (open / parse / wrong version).
        std::vector<std::string> errors;
        // Per-type schema diagnostics from deserialize_registry.
        std::vector<std::string> warnings;
    };

    // Loads a single registry-pack JSON, merging types into `reg`.
    NodePackLoadResult load_node_pack(std::string const& path,
                                       piper::NodeRegistry& reg);

    // Scans `nodes_dir()` for *.json files (non-recursive, sorted) and
    // merges each into `reg`. Empty/missing dir returns a zeroed result.
    NodePackLoadResult auto_load_node_packs(piper::NodeRegistry& reg);
}

#endif
