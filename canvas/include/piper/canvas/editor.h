#ifndef PIPER_CANVAS_EDITOR_H
#define PIPER_CANVAS_EDITOR_H

#include <cstddef>
#include <functional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <imgui.h>

#include "piper/canvas/event.h"
#include "piper/canvas/graph.h"
#include "piper/canvas/ids.h"
#include "piper/canvas/selection.h"
#include "piper/canvas/style.h"
#include "piper/canvas/transform.h"

namespace piper::canvas
{
    class Graph;

    // Contract: callbacks must NOT call Begin/End, OpenPopup/BeginPopup,
    // or push style stacks that outlive the call. PushClipRect is
    // permitted if matched by PopClipRect.
    using BodyRenderer = std::function<void(NodeId, ImDrawList*, ImVec2 const& rect_min, ImVec2 const& rect_max)>;

    using ContextMenuFn = std::function<void(NodeId hovered, ImVec2 const& canvas_pos)>;

    class Editor
    {
    public:
        explicit Editor(Graph& source);

        Editor(Editor const&)            = delete;
        Editor& operator=(Editor const&) = delete;
        Editor(Editor&&)                 = delete;
        Editor& operator=(Editor&&)      = delete;

        // draw() reserves `size` as an InvisibleButton for input
        // capture. Anything the host renders before draw() in the same
        // window is drawn behind the canvas; anything after is in
        // front but does not receive input on the canvas rect.
        void draw(ImVec2 const& size);

        // Spans returned here are valid only until the next draw() or
        // consume_events() call. Hosts must copy any data they need
        // beyond the current frame (e.g. clipboard snapshots).
        std::span<Event const> consume_events();

        void         set_style(Style const& style) { style_ = style; }
        Style const& style() const                 { return style_; }

        void set_body_renderer(BodyRenderer const& renderer) { body_renderer_ = renderer; }
        void set_context_menu(ContextMenuFn const& menu)     { context_menu_  = menu; }

        // Imperative API for host-driven view changes. PR 2.2+ implements.
        void   center_on(NodeId id);
        void   scroll_to(NodeId id);
        void   set_selection(std::span<NodeId const> ids);
        ImVec2 screen_to_canvas(ImVec2 const& screen) const;
        ImVec2 canvas_to_screen(ImVec2 const& canvas) const;

    private:
        struct PinLocation
        {
            NodeId      node_id;
            PinKind     kind;
            std::size_t index;
            ImVec2      center;   // canvas-space, drag-offset-adjusted
            Pin const*  pin;      // valid for the current frame only
        };

        Graph&             source_;
        Style              style_;
        BodyRenderer       body_renderer_;
        ContextMenuFn      context_menu_;
        std::vector<Event> pending_events_;
        std::vector<Event> drained_events_;
        Transform          transform_;
        // Cached top-left of the drawable rect from the last draw().
        // screen_to_canvas / canvas_to_screen use it; calling them
        // before the first draw() returns the unset {0,0} origin.
        ImVec2             last_origin_{0.0f, 0.0f};
        // Rebuilt at the top of every draw() — link rendering and
        // hit-testing look up pin centers by id here.
        std::unordered_map<PinId, PinLocation> pin_index_;

        Selection          selection_;
        // Snapshot of selection_ before a box-select drag started.
        // Shift-box-select unions with this; non-shift replaces it.
        std::vector<NodeId> box_select_base_;
        bool                box_selecting_{false};
        bool                box_select_additive_{false};
        ImVec2              box_start_canvas_{0.0f, 0.0f};
        ImVec2              box_current_canvas_{0.0f, 0.0f};

        // Drag-to-move state. drag_start_positions_ snapshots the
        // selection's positions at click time; on release we emit one
        // NodeMoved per entry with start + drag_delta_.
        bool                                   dragging_nodes_{false};
        ImVec2                                 drag_start_canvas_{0.0f, 0.0f};
        ImVec2                                 drag_delta_{0.0f, 0.0f};
        std::vector<std::pair<NodeId, ImVec2>> drag_start_positions_;

        // Click-on-already-selected without shift defers the
        // reduce-to-single until release-without-drag, so the user can
        // still drag a multi-selection by clicking any of its members.
        bool   pending_reduce_to_single_{false};
        NodeId pending_reduce_node_{};

        // Drag-to-connect state. The source pin id is stable across
        // frames; pin_index_ resolves it to a Pin pointer each frame.
        bool    connecting_{false};
        PinId   connect_from_pin_id_{};
        PinKind connect_from_kind_{};
        NodeId  connect_from_node_id_{};
    };
}

#endif
