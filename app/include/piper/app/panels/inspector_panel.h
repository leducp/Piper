#ifndef PIPER_APP_PANELS_INSPECTOR_PANEL_H
#define PIPER_APP_PANELS_INSPECTOR_PANEL_H

#include "piper/command_stack.h"
#include "piper/graph.h"
#include "piper/node.h"

namespace piper::app
{
    // Renders editable fields for the currently-selected node.
    // Mutations are pushed through the host's CommandStack so they
    // participate in undo with the rest of the editor's edits.
    class InspectorPanel
    {
    public:
        // Returns true when at least one command was pushed this
        // frame, so the host can rebuild any view caches (e.g. the
        // canvas adapter).
        bool draw(piper::Graph&        graph,
                  piper::CommandStack& stack,
                  NodeId               selected);
    };
}

#endif
