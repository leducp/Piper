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
    // remove_stage does NOT cascade -- references in Node::stage and
    // Attribute::stages survive verbatim and are surfaced as
    // UnknownStageLabel diagnostics at load time.
    class Graph
    {
    public:
        // Synthesizes Attributes from type.attributes (value defaulted from
        // spec, stages left empty so they inherit Node::stage).
        NodeId add_node(NodeType const& type,
                        std::string const& name,
                        std::string const& stage,
                        Point pos);

        // Re-insert a fully-formed Node (used by deserialize and undo to
        // restore an exact id). Returns false if the id already exists.
        bool insert_node(Node const& node);

        // No-op if id is unknown.
        void remove_node(NodeId id);

        // Returns invalid_link_id if either endpoint cannot be resolved.
        // Type compatibility is the caller's responsibility.
        LinkId add_link(PinRef const& from, PinRef const& to, std::string const& data_type);

        // Re-insert a fully-formed Link. Returns false if the id already
        // exists or either endpoint cannot be resolved on the graph.
        bool insert_link(Link const& link);

        void remove_link(LinkId id);

        // Per-attribute mutators. Return false if the node or named
        // attribute does not exist.
        bool set_attr_value(NodeId id,
                            std::string_view attr_name,
                            std::string const& value);
        bool set_attr_stages(NodeId id,
                             std::string_view attr_name,
                             std::vector<std::string> const& stages);

        // Per-node mutators. Return false if the node does not exist.
        bool move_node(NodeId id, Point pos);
        bool set_node_stage(NodeId id, std::string const& stage);
        bool rename_node(NodeId id, std::string const& new_name);

        // Returns false on duplicate name; existing entry kept.
        bool add_stage(Stage const& stage);
        // No-op if name is unknown. Does NOT cascade to nodes.
        void remove_stage(std::string_view name);

        // Adjacent-swap reorder helpers. Return false when name is
        // unknown OR the stage is already at the relevant boundary.
        bool move_stage_up(std::string_view name);
        bool move_stage_down(std::string_view name);

        // Drops `name` at the slot currently occupied by `target`
        // (drag-drop semantics): if `name` was above `target`, it
        // lands just after `target`'s old position; if `name` was
        // below, it lands just before. Empty / unknown `target`
        // appends `name` to the end. Returns false if `name` is
        // unknown or `name == target`.
        bool move_stage_to(std::string_view name, std::string_view target);

        // Re-insert a stage at a specific index. Used by undo to
        // restore a removed stage at its original position. Returns
        // false on duplicate name. Index is clamped into range.
        bool insert_stage_at(Stage const& stage, std::size_t index);

        // Reorder stages in place to match `order` (names only).
        // Names not present in the current stage list are skipped;
        // current stages whose name is not in `order` are appended
        // to the end in their existing relative order. Used by undo
        // for reorder commands.
        void set_stages_order(std::vector<std::string> const& order);

        // Re-insert a profile at a specific index. Used by undo to
        // restore a removed profile at its original position.
        // Returns false on duplicate name.
        bool insert_mode_profile_at(ModeProfile const& profile, std::size_t index);

        // Returns false on duplicate name; existing entry kept.
        bool add_mode_profile(ModeProfile const& profile);
        // No-op if name is unknown.
        void remove_mode_profile(std::string_view name);

        // In-place edit of a single (profile, node) -> label cell.
        // Empty label erases the entry. Preserves profile order in
        // the graph (unlike remove + add which would reorder).
        // Returns false if the profile is unknown.
        bool set_node_mode_label(std::string_view  profile,
                                  NodeId            node_id,
                                  std::string const& label);

        std::vector<Node>        const& nodes()         const { return nodes_;  }
        std::vector<Link>        const& links()         const { return links_;  }
        std::vector<Stage>       const& stages()        const { return stages_; }
        std::vector<ModeProfile> const& mode_profiles() const { return modes_;  }

        Node const* find_node(NodeId id) const;
        Link const* find_link(LinkId id) const;

        // Returns nullptr if not found.
        Node* find_node_mut(NodeId id);

        // Forces the next-id counters past the given values. Used by
        // deserialize after loading a graph with sparse / non-monotonic ids.
        void reserve_ids_above(NodeId max_node_id, LinkId max_link_id);

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
