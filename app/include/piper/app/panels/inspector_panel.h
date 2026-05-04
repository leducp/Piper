#ifndef PIPER_APP_PANELS_INSPECTOR_PANEL_H
#define PIPER_APP_PANELS_INSPECTOR_PANEL_H

#include <string>

#include "piper/command_stack.h"
#include "piper/graph.h"
#include "piper/node.h"
#include "piper/registry.h"
#include "piper/theme.h"

namespace piper::app
{
    class InspectorPanel
    {
    public:
        // Returns true if the graph was mutated this frame.
        bool draw(piper::Graph&              graph,
                  piper::NodeRegistry const& registry,
                  piper::CommandStack&       stack,
                  NodeId                     selected,
                  piper::Theme const&        theme,
                  std::string const&         active_mode_profile);
    };
}

#endif
