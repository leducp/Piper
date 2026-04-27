#ifndef PIPER_CANVAS_EDITOR_H
#define PIPER_CANVAS_EDITOR_H

#include <functional>
#include <span>
#include <vector>

#include <imgui.h>

#include "piper/canvas/event.h"
#include "piper/canvas/ids.h"
#include "piper/canvas/style.h"

namespace piper::canvas
{
    class Graph;

    // Contract: callbacks must NOT call Begin/End, OpenPopup/BeginPopup,
    // or push style stacks that outlive the call. PushClipRect is
    // permitted if matched by PopClipRect.
    using BodyRenderer = std::function<void(NodeId, ImDrawList*, ImVec2 rect_min, ImVec2 rect_max)>;

    using ContextMenuFn = std::function<void(NodeId hovered, ImVec2 canvas_pos)>;

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
        void draw(ImVec2 size);

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
        ImVec2 screen_to_canvas(ImVec2 screen) const;
        ImVec2 canvas_to_screen(ImVec2 canvas) const;

    private:
        Graph&             source_;
        Style              style_;
        BodyRenderer       body_renderer_;
        ContextMenuFn      context_menu_;
        std::vector<Event> pending_events_;
        std::vector<Event> drained_events_;
    };
}

#endif
