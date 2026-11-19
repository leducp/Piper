#ifndef PIPER_APP_PANELS_PROFILES_PANEL_H
#define PIPER_APP_PANELS_PROFILES_PANEL_H

#include <array>
#include <string>

#include "piper/command_stack.h"
#include "piper/graph.h"
#include "piper/registry.h"

namespace piper::studio
{
    // One fold per profile listing each step's mode. Edits go through
    // the CommandStack.
    class ProfilesPanel
    {
    public:
        // Returns true when graph profiles or the active profile
        // selection mutated this frame.
        bool draw(piper::Graph&              graph,
                  piper::NodeRegistry const& registry,
                  piper::CommandStack&       stack,
                  std::string&               active_profile);

    private:
        // Persistent edit buffer for the "Add profile" InputText so
        // the text survives across frames; cleared on Add.
        std::array<char, 64> add_buf_{};
    };
}

#endif
