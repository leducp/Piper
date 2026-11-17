#ifndef PIPER_APP_PANELS_MODE_PICKER_H
#define PIPER_APP_PANELS_MODE_PICKER_H

#include <optional>
#include <string>

#include "piper/node.h"
#include "piper/registry.h"

namespace piper::studio
{
    // Draws the modes `node` accepts as items of an open combo or menu.
    // Returns the picked mode.
    std::optional<std::string> draw_mode_items(piper::Node const&         node,
                                               piper::NodeRegistry const& registry,
                                               std::string const&         current_mode);

    std::optional<std::string> mode_combo(char const*                label,
                                          piper::Node const&         node,
                                          piper::NodeRegistry const& registry,
                                          std::string const&         current_mode);
}

#endif
