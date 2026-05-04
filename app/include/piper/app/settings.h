#ifndef PIPER_APP_SETTINGS_H
#define PIPER_APP_SETTINGS_H

#include <optional>
#include <string>
#include <vector>

namespace piper::studio
{
    struct Settings
    {
        std::optional<std::string>              font_path;
        std::optional<float>                    font_size;
        std::optional<int>                      window_x;
        std::optional<int>                      window_y;
        std::optional<int>                      window_w;
        std::optional<int>                      window_h;
        // Recent files MRU. Most-recent first; capped at ~10 by callers.
        std::optional<std::vector<std::string>> recent_files;
    };

    // Path of the user-settings JSON file (XDG-aware). Empty if neither
    // $XDG_CONFIG_HOME nor $HOME is set.
    std::string settings_path();

    // Reads `settings_path()`. Missing file -> empty Settings; malformed
    // JSON -> empty Settings + a stderr warning.
    Settings load_settings();

    // Writes the JSON to `settings_path()`. Best-effort: failures log to
    // stderr but do not throw.
    void save_settings(Settings const& settings);
}

#endif
