#ifndef PIPER_APP_PANELS_MODES_PANEL_H
#define PIPER_APP_PANELS_MODES_PANEL_H

#include <array>
#include <string>

#include "piper/command_stack.h"
#include "piper/graph.h"
#include "piper/theme.h"

namespace piper::app
{
    // Mode-profile CRUD + active-profile selector. Profile and per-
    // node label mutations flow through the host's CommandStack so
    // they undo with the rest. The active profile drives the body-
    // color overlay through PiperCanvasGraph::set_active_mode_profile.
    class ModesPanel
    {
    public:
        // Returns true when graph mode profiles or the active profile
        // selection mutated this frame.
        bool draw(piper::Graph&        graph,
                  piper::CommandStack& stack,
                  piper::Theme const&  theme,
                  std::string&         active_profile);

    private:
        // Persistent edit buffer for the "Add profile" InputText so
        // the text survives across frames; cleared on Add.
        std::array<char, 64> add_buf_{};
    };
}

#endif
