#ifndef PIPER_APP_PANELS_INSPECTOR_PANEL_H
#define PIPER_APP_PANELS_INSPECTOR_PANEL_H

#include <array>
#include <string>

#include "piper/command_stack.h"
#include "piper/graph.h"
#include "piper/node.h"
#include "piper/registry.h"

namespace piper::studio
{
    class InspectorPanel
    {
    public:
        // Returns true if the graph was mutated this frame.
        bool draw(piper::Graph&              graph,
                  piper::NodeRegistry const& registry,
                  piper::CommandStack&       stack,
                  NodeId                     selected,
                  std::string const&         active_profile);

    private:
        // The node's mode list; the type's modes cannot be renamed or removed.
        bool draw_modes(piper::Graph&          graph,
                        piper::CommandStack&   stack,
                        piper::Node const&     node,
                        piper::NodeType const* type);

        // Default value, then one row per mode overload.
        bool draw_member(piper::Graph&           graph,
                         piper::CommandStack&    stack,
                         piper::Node const&      node,
                         piper::Attribute const& attr);

        NodeId                 note_buf_node_{invalid_node_id};
        std::array<char, 1024> note_buf_{};
        std::array<char, 64>   new_mode_buf_{};
    };
}

#endif
