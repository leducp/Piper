#ifndef PIPER_CANVAS_EVENT_H
#define PIPER_CANVAS_EVENT_H

#include <vector>

#include <imgui.h>

#include "piper/canvas/ids.h"

namespace piper::canvas
{
    enum class Event
    {
        NodeMoved,             // node + pos
        NodeDeleted,           // node
        LinkCreated,           // pin_from + pin_to
        LinkDeleted,           // link
        SelectionChanged,      // selection
        ContextMenuRequested,  // node (or invalid_*) + pos
        DoubleClicked,         // node (or invalid_*) + pos
        CopyRequested,         // selection
        PasteRequested,        // pos
        CutRequested,          // selection
        DuplicateRequested,    // selection
        UndoRequested,         // (no payload)
        RedoRequested,         // (no payload)
        // Host-owned (annotation) drag lifecycle. Editor emits these
        // when set_extra_hit_test claims a left mouse-down on empty
        // canvas; the host applies the drag to its own entities.
        ExtraDragStarted,      // pos (canvas-space mouse-down position)
        ExtraDragMoved,        // pos (current canvas-space cursor)
        ExtraDragEnded,        // pos (release canvas-space cursor)
    };

    // Tagged-union payload for events the canvas emits to the host.
    // Which fields are valid depends on `kind`; see the comments above.
    // selection is a value vector (not a span over framework-internal
    // state) so payload-bearing events stay valid after subsequent
    // selection mutations within the same draw().
    struct EventPayload
    {
        Event                kind;
        NodeId               node;
        ImVec2               pos;
        PinId                pin_from;
        PinId                pin_to;
        LinkId               link;
        std::vector<NodeId>  selection;
    };
}

#endif
