#ifndef PIPER_APP_PANELS_STAGES_PANEL_H
#define PIPER_APP_PANELS_STAGES_PANEL_H

#include <string>

#include "piper/graph.h"

namespace piper::app
{
    // Stage CRUD + current-stage selector. The selected stage drives
    // pin/link dimming through PiperCanvasGraph::set_current_stage.
    class StagesPanel
    {
    public:
        // Returns true when the graph's stage list mutated this frame
        // (host should rebuild the canvas adapter to reflect dim
        // updates that depend on stage membership).
        bool draw(piper::Graph& graph, std::string& current_stage);
    };
}

#endif
