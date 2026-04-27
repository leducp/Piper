#ifndef PIPER_MODE_PROFILE_H
#define PIPER_MODE_PROFILE_H

#include <string>
#include <unordered_map>

namespace piper
{
    struct ModeProfile
    {
        std::string name;
        bool        is_default{false};
        // Built-ins: "enable", "disable". Other labels are opaque to V2;
        // resolution happens in app via mode_color_table.
        std::unordered_map<std::string, std::string> per_node;
    };
}

#endif
