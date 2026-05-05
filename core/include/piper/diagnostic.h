#ifndef PIPER_DIAGNOSTIC_H
#define PIPER_DIAGNOSTIC_H

#include <string>

#include "piper/link.h"
#include "piper/node.h"

namespace piper
{
    struct Diagnostic
    {
        enum class Kind
        {
            SchemaError,                // malformed entry; the entry was skipped
            DuplicateNodeId,
            DuplicateLinkId,
            DuplicateStageName,
            DuplicateProfileName,
            DuplicateTypeName,          // duplicate registry type name
            UnknownNodeType,            // node.type not in registry
            AttributeMissing,           // saved attribute not in current registry spec
            AttributeAdded,             // registry spec has attribute not in saved node
            AttributeDrift,             // saved attribute's data_type differs from spec
            LinkOrphanedNode,           // link references a node not in the graph
            LinkOrphanedAttribute,      // link references an attribute not on the node
            LinkTypeMismatch,           // link.data_type differs from endpoint data_types
            OrphanModeReference,        // mode profile references a node id not in the graph
            UnknownStageReference,      // Node::stage or Attribute::stages references unknown stage
            // Informational: load-time repair replaced one or more
            // label colors so each name-cluster shares a single color.
            // Not an error -- emitted only when the file already
            // disagreed with the invariant.
            LabelClusterRepaired,
        };

        Kind        kind;
        std::string message;     // always populated; human-readable

        // Locator fields. Default to invalid_*_id / empty when not applicable.
        NodeId      node_id{invalid_node_id};
        std::string attr_name;
        LinkId      link_id{invalid_link_id};
    };
}

#endif
