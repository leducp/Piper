#include "piper/canvas/editor.h"

#include "piper/canvas/graph.h"

namespace piper::canvas
{
    Editor::Editor(Graph& source)
        : source_(source)
    {
    }

    void Editor::draw(ImVec2 size)
    {
        // PR 2.2: pan/zoom + grid background.
        // PR 2.3+: nodes, pins, links, interaction.
        (void)size;
    }

    std::span<Event const> Editor::consume_events()
    {
        // Swap so the returned span remains valid until the next call.
        drained_events_.clear();
        pending_events_.swap(drained_events_);
        return drained_events_;
    }

    void Editor::center_on(NodeId id)
    {
        // PR 2.2 (transform).
        (void)id;
    }

    void Editor::scroll_to(NodeId id)
    {
        // PR 2.2 (transform).
        (void)id;
    }

    void Editor::set_selection(std::span<NodeId const> ids)
    {
        // PR 2.5 (selection).
        (void)ids;
    }

    ImVec2 Editor::screen_to_canvas(ImVec2 screen) const
    {
        // PR 2.2 (transform).
        return screen;
    }

    ImVec2 Editor::canvas_to_screen(ImVec2 canvas) const
    {
        // PR 2.2 (transform).
        return canvas;
    }
}
