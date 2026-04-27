#ifndef PIPER_GRAPH_H
#define PIPER_GRAPH_H

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "piper/link.h"
#include "piper/mode_profile.h"
#include "piper/node.h"
#include "piper/node_type.h"
#include "piper/stage.h"

namespace piper
{
    // remove_node cascades to incident links.
    // remove_stage cascades: clears stage name from any node that referenced it.
    class Graph
    {
    public:
        // Synthesizes Attributes from type.attributes (value defaulted from
        // spec, stages left empty so they inherit Node::stage).
        NodeId add_node(NodeType const& type,
                        std::string const& name,
                        std::string const& stage,
                        Point pos);

        // No-op if id is unknown.
        void remove_node(NodeId id);

        // Returns invalid_link_id if either endpoint cannot be resolved.
        // Type compatibility is the caller's responsibility.
        LinkId add_link(PinRef const& from, PinRef const& to, std::string const& data_type);

        void remove_link(LinkId id);

        void add_stage(Stage const& stage);
        void remove_stage(std::string_view name);

        void add_mode_profile(ModeProfile const& profile);
        void remove_mode_profile(std::string_view name);

        std::vector<Node>        const& nodes()         const { return nodes_;  }
        std::vector<Link>        const& links()         const { return links_;  }
        std::vector<Stage>       const& stages()        const { return stages_; }
        std::vector<ModeProfile> const& mode_profiles() const { return modes_;  }

        Node const* find_node(NodeId id) const;
        Link const* find_link(LinkId id) const;

        // Returns nullptr if not found.
        Node* find_node_mut(NodeId id);

        NodeId peek_next_node_id() const { return next_node_id_; }
        LinkId peek_next_link_id() const { return next_link_id_; }

    private:
        bool resolve_pin(PinRef const& ref) const;

        std::vector<Node>        nodes_;
        std::vector<Link>        links_;
        std::vector<Stage>       stages_;
        std::vector<ModeProfile> modes_;

        // 0 reserved as invalid_node_id / invalid_link_id.
        NodeId next_node_id_{1};
        LinkId next_link_id_{1};
    };
}

#endif
