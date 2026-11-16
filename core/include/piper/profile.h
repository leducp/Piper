#ifndef PIPER_PROFILE_H
#define PIPER_PROFILE_H

#include <string>
#include <unordered_map>

#include "piper/node.h"

namespace piper
{
    struct Profile
    {
        std::string name;
        // Modes by node. The engine skips "disable"; steps interpret custom
        // modes. The editor resolves their colors through Theme::mode_colors.
        std::unordered_map<NodeId, std::string> per_node;
    };
}

#endif
