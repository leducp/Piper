#ifndef PIPER_APP_PANELS_INSPECTOR_PANEL_H
#define PIPER_APP_PANELS_INSPECTOR_PANEL_H

#include <string>

#include "piper/command_stack.h"
#include "piper/graph.h"
#include "piper/node.h"
#include "piper/theme.h"

namespace piper::app
{
    // Renders editable fields for the currently-selected node.
    // Name/stage/member-attribute mutations push through the host's
    // CommandStack (undoable). Mode-label changes direct-mutate the
    // active profile for now (a SetModeProfileCommand is pending).
    class InspectorPanel
    {
    public:
        // Returns true when the inspector mutated the graph this
        // frame, so the host can rebuild view caches (e.g. the
        // canvas adapter).
        bool draw(piper::Graph&        graph,
                  piper::CommandStack& stack,
                  NodeId               selected,
                  piper::Theme const&  theme,
                  std::string const&   active_mode_profile);
    };
}

#endif
