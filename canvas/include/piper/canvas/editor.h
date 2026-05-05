#ifndef PIPER_CANVAS_EDITOR_H
#define PIPER_CANVAS_EDITOR_H

#include <cstddef>
#include <functional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include <imgui.h>

#include "piper/canvas/cull.h"
#include "piper/canvas/event.h"
#include "piper/canvas/graph.h"
#include "piper/canvas/ids.h"
#include "piper/canvas/selection.h"
#include "piper/canvas/style.h"
#include "piper/canvas/transform.h"

namespace piper::canvas
{
    class Graph;

    // Called once per visible node, after the body bg / header /
    // outline are drawn but before pins. `rect_min`/`rect_max`
    // delimit the "extra content" rect -- the screen-space area
    // *below* the pin rows, sized from `Node::body_min_size.y`.
    // Pins are drawn separately by the framework; host content does
    // not overlap them. `zoom` is the current canvas zoom -- hosts
    // use it to scale text via
    // ImDrawList::AddText(font, font_size * zoom, ...) or to hide
    // fixed-size ImGui widgets when the node is too small to be
    // useful. May call ImDrawList primitives or ImGui widgets, with
    // PushClipRect matched by PopClipRect. Must NOT call Begin/End,
    // OpenPopup, or leave style stacks open.
    using BodyRenderer = std::function<void(NodeId,
                                             ImDrawList*,
                                             ImVec2 const& rect_min,
                                             ImVec2 const& rect_max,
                                             float         zoom)>;

    // Drawn after the grid and before nodes, so node bodies cover any
    // overlap. Host renders host-defined decoration (annotations, frames).
    // origin/size delimit the canvas-screen rect.
    using BackgroundRenderer = std::function<void(ImDrawList*,
                                                   ImVec2 const& origin,
                                                   ImVec2 const& size,
                                                   float         zoom,
                                                   ImVec2 const& pan)>;

    // Invoked inside an active ImGui popup, once per frame the canvas
    // popup is open. Host adds MenuItem / Selectable / Separator calls
    // and must NOT call BeginPopup/EndPopup itself. `hovered` is
    // invalid_node_id when the right-click landed on empty canvas.
    using ContextMenuFn = std::function<void(NodeId hovered, ImVec2 const& canvas_pos)>;

    // Asks the host whether it owns an interaction at the given
    // canvas-space position (typically: is there an annotation under
    // the cursor?). When the host returns true on a left mouse-down
    // landing on empty canvas, the editor suppresses its own
    // box-select and emits ExtraDrag* events instead so the host can
    // drive the drag.
    using ExtraHitTestFn = std::function<bool(ImVec2 const& canvas_pos)>;

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
        std::span<EventPayload const> consume_events();

        void         set_style(Style const& style) { style_ = style; }
        Style const& style() const                 { return style_; }

        void                 set_layout(LayoutMetrics const& m) { layout_ = m; }
        LayoutMetrics const& layout() const                     { return layout_; }

        void set_body_renderer(BodyRenderer const& renderer)     { body_renderer_ = renderer; }
        void set_background_renderer(BackgroundRenderer const& r){ background_renderer_ = r; }
        void set_context_menu(ContextMenuFn const& menu)         { context_menu_  = menu; }
        void set_extra_hit_test(ExtraHitTestFn const& fn)        { extra_hit_test_ = fn; }

        // Imperative API for host-driven view changes.
        void   center_on(NodeId id);
        void   center_on(ImVec2 const& canvas_pos);
        void   scroll_to(NodeId id);
        void   set_selection(std::span<NodeId const> ids);
        std::span<NodeId const> selection_ids() const { return selection_.ids(); }
        ImVec2 screen_to_canvas(ImVec2 const& screen) const;
        ImVec2 canvas_to_screen(ImVec2 const& canvas) const;

        // Pan + zoom so the AABB of `ids` (or all nodes if empty) fills
        // the viewport with margin. No-op if there are no nodes to fit
        // or if the editor hasn't been drawn yet (viewport size unknown).
        void zoom_to_fit(std::span<NodeId const> ids = {});

        // Defers a zoom_to_fit to the next draw() — useful right after
        // a document load when the canvas hasn't measured itself yet.
        void request_fit(std::span<NodeId const> ids = {});

        // Aggregate view state for status / overlay readouts. The
        // origin/size members are zero before the first draw().
        struct Viewport
        {
            ImVec2 origin_screen{0.0f, 0.0f};
            ImVec2 size_screen{0.0f, 0.0f};
            ImVec2 pan{0.0f, 0.0f};
            float  zoom{1.0f};
        };
        Viewport viewport() const
        {
            return Viewport{ last_origin_, last_size_, transform_.pan, transform_.zoom };
        }
        float  zoom() const               { return transform_.zoom; }
        // Node currently under the cursor (last draw); invalid when none.
        NodeId hovered_node() const       { return last_hovered_node_; }
        LinkId selected_link() const      { return selected_link_; }

    private:
        // Canvas-space pin hit radius. Combines the visible pin
        // radius with a screen-space floor so clicks stay easy when
        // the canvas is zoomed out.
        float pin_hit_radius() const;

        // Returns the LinkId whose bezier passes within `tolerance`
        // screen pixels of `mouse_screen`, or invalid_link_id. Uses
        // the current frame's pin_index_ so it must be called after
        // the pin index has been rebuilt during draw().
        LinkId hit_test_link_at(ImVec2 mouse_screen, ImVec2 const& origin,
                                 float tolerance) const;

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
        LayoutMetrics      layout_;
        BodyRenderer       body_renderer_;
        BackgroundRenderer background_renderer_;
        ContextMenuFn      context_menu_;
        ExtraHitTestFn     extra_hit_test_;
        std::vector<EventPayload> pending_events_;
        std::vector<EventPayload> drained_events_;
        Transform          transform_;
        // Cached top-left and size of the drawable rect from the last
        // draw(). screen_to_canvas / canvas_to_screen and the
        // viewport math in center_on use it; calling them before
        // the first draw() returns the unset (0,0) values.
        ImVec2             last_origin_{0.0f, 0.0f};
        ImVec2             last_size_{0.0f, 0.0f};
        // Rebuilt at the top of every draw() -- link rendering and
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
        // NodeMoved per entry with start + drag_delta_. The lead
        // node's start position is the anchor for grid snapping so
        // it lands on the absolute grid; the rest of the selection
        // preserves its relative offset.
        bool                                   dragging_nodes_{false};
        ImVec2                                 drag_start_canvas_{0.0f, 0.0f};
        ImVec2                                 drag_delta_{0.0f, 0.0f};
        ImVec2                                 drag_lead_start_pos_{0.0f, 0.0f};
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

        // Host-owned drag (annotations). Active between ExtraDragStarted
        // and ExtraDragEnded; suppresses box-select for the duration.
        bool   extra_dragging_{false};
        ImVec2 extra_drag_last_canvas_{0.0f, 0.0f};

        // Context-menu state. Populated on right-click; consumed by the
        // BeginPopup wrapper so the host callback runs inside the popup
        // window each frame the popup is open.
        NodeId  context_menu_node_{};
        ImVec2  context_menu_canvas_{0.0f, 0.0f};

        // Deferred zoom-to-fit consumed at the start of the next draw().
        bool                pending_fit_{false};
        std::vector<NodeId> pending_fit_ids_;

        // Node under the cursor at the end of the last draw().
        NodeId              last_hovered_node_{};

        // Currently selected link, if any. Cleared when the user clicks
        // on a node, pin, or empty canvas; cleared by Delete after the
        // LinkDeleted event has been queued.
        LinkId              selected_link_{};
    };
}

#endif
