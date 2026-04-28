#ifndef PIPER_APP_PANELS_MODES_PANEL_H
#define PIPER_APP_PANELS_MODES_PANEL_H

#include <string>

#include "piper/graph.h"
#include "piper/theme.h"

namespace piper::app
{
    // Mode-profile CRUD + active-profile selector. The active profile
    // drives the body-color overlay through PiperCanvasGraph::
    // set_active_mode_profile.
    class ModesPanel
    {
    public:
        // Returns true when graph mode profiles or the active profile
        // selection mutated this frame.
        bool draw(piper::Graph&        graph,
                  piper::Theme const&  theme,
                  std::string&         active_profile);
    };
}

#endif
