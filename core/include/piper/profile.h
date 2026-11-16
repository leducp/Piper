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
        // Mode of each node in this profile. Built-ins: "enable",
        // "disable"; other modes are opaque to V2;
        // resolution happens in app via mode_color_table.
        std::unordered_map<NodeId, std::string> per_node;
    };
}

#endif
