#include <imgui.h>

#include "piper/app/panels/mode_picker.h"

#include "piper/modes.h"

namespace piper::studio
{
    std::optional<std::string> draw_mode_items(piper::Node const&         node,
                                               piper::NodeRegistry const& registry,
                                               std::string const&         current_mode)
    {
        std::optional<std::string> picked;
        for (auto const& mode : piper::modes_of(node, registry))
        {
            bool const sel = (current_mode == mode);
            if (ImGui::Selectable(mode.c_str(), sel) and not sel)
            {
                picked = mode;
            }
        }
        return picked;
    }

    std::optional<std::string> mode_combo(char const*                label,
                                          piper::Node const&         node,
                                          piper::NodeRegistry const& registry,
                                          std::string const&         current_mode)
    {
        char const* preview = "(unset)";
        if (not current_mode.empty())
        {
            preview = current_mode.c_str();
        }
        std::optional<std::string> picked;
        if (ImGui::BeginCombo(label, preview))
        {
            picked = draw_mode_items(node, registry, current_mode);
            ImGui::EndCombo();
        }
        return picked;
    }
}
