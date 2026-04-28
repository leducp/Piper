#ifndef PIPER_CANVAS_EVENT_H
#define PIPER_CANVAS_EVENT_H

#include <span>

#include <imgui.h>

#include "piper/canvas/ids.h"

namespace piper::canvas
{
    enum class EventKind
    {
        NodeMoved,             // node + pos
        NodeDeleted,           // node
        LinkCreated,           // pin_from + pin_to
        LinkDeleted,           // link
        SelectionChanged,      // selection
        ContextMenuRequested,  // node (or invalid_*) + pos
        CopyRequested,         // selection
        PasteRequested,        // pos
        UndoRequested,         // (no payload)
        RedoRequested,         // (no payload)
    };

    struct Event
    {
        EventKind                 kind;
        NodeId                    node;
        ImVec2                    pos;
        PinId                     pin_from;
        PinId                     pin_to;
        LinkId                    link;
        std::span<NodeId const>   selection;
    };
}

#endif
