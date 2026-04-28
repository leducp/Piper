#ifndef PIPER_APP_CANVAS_ADAPTER_H
#define PIPER_APP_CANVAS_ADAPTER_H

#include <cstdint>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "piper/canvas/graph.h"
#include "piper/canvas/ids.h"
#include "piper/graph.h"
#include "piper/link.h"
#include "piper/registry.h"
#include "piper/theme.h"

namespace piper::app
{
    // Bridges a piper::Graph + NodeRegistry into a canvas::Graph the
    // editor can render. View-only at PR 4.2: rebuild() is called
    // explicitly after host mutations.
    class PiperCanvasGraph : public canvas::Graph
    {
    public:
        PiperCanvasGraph(piper::Graph const&        graph,
                         piper::NodeRegistry const& registry,
                         piper::Theme const&        theme);

        void rebuild();

        // Setting a non-empty stage triggers per-pin dimming on the
        // next rebuild: pins whose attribute is not active in the
        // given stage drop to theme.pin_alpha_inactive, and links
        // inherit the minimum alpha of their two endpoints.
        void set_current_stage(std::string const& stage_name);
        std::string const& current_stage() const { return current_stage_; }

        std::span<canvas::Node const> nodes() const override { return mirror_nodes_; }
        std::span<canvas::Link const> links() const override { return mirror_links_; }

        canvas::Connect can_connect(canvas::Pin const& a,
                                    canvas::Pin const& b) const override;

        // Maps between framework pin ids and domain (node, attr) refs.
        // Returns invalid_pin_id / an empty PinRef for unknowns.
        canvas::PinId  ref_to_pin_id(PinRef const& ref) const;
        PinRef         pin_id_to_ref(canvas::PinId id) const;

    private:
        struct PinKey
        {
            NodeId      node;
            std::string attr;

            bool operator==(PinKey const& other) const
            {
                return node == other.node and attr == other.attr;
            }
        };

        struct PinKeyHash
        {
            std::size_t operator()(PinKey const& k) const noexcept
            {
                std::size_t const h1 = std::hash<uint64_t>{}(k.node);
                std::size_t const h2 = std::hash<std::string>{}(k.attr);
                return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
            }
        };

        piper::Graph const&        graph_;
        piper::NodeRegistry const& registry_;
        piper::Theme const&        theme_;

        std::vector<canvas::Node>             mirror_nodes_;
        std::vector<canvas::Link>             mirror_links_;
        std::vector<std::vector<canvas::Pin>> inputs_;
        std::vector<std::vector<canvas::Pin>> outputs_;

        std::unordered_map<PinKey, canvas::PinId, PinKeyHash> forward_;
        std::unordered_map<uint64_t, PinRef>                  reverse_;
        uint64_t                                              next_pin_id_{1};
        std::string                                           current_stage_;
    };
}

#endif
