#ifndef PIPER_APP_PANELS_STAGES_PANEL_H
#define PIPER_APP_PANELS_STAGES_PANEL_H

#include <array>
#include <string>

#include "piper/command_stack.h"
#include "piper/graph.h"

namespace piper::app
{
    // Stage CRUD + current-stage selector. Stage mutations flow
    // through the host's CommandStack so they undo with the rest.
    // The selected stage drives pin/link dimming through
    // PiperCanvasGraph::set_current_stage.
    class StagesPanel
    {
    public:
        // Returns true when the graph's stage list mutated this frame
        // (host should rebuild the canvas adapter to reflect dim
        // updates that depend on stage membership).
        bool draw(piper::Graph&        graph,
                  piper::CommandStack& stack,
                  std::string&         current_stage);

    private:
        std::array<char, 64> add_buf_{};
        std::string          editing_color_;
    };
}

#endif
