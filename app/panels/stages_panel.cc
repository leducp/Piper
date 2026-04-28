#include "piper/app/panels/stages_panel.h"

#include <cstring>
#include <string>

#include <imgui.h>

#include "piper/stage.h"

namespace piper::app
{
    bool StagesPanel::draw(piper::Graph& graph, std::string& current_stage)
    {
        bool dirty = false;

        ImGui::TextUnformatted("Stages");
        ImGui::Separator();

        // Current-stage combo. "(all)" disables dimming.
        char const* preview = current_stage.empty() ? "(all)" : current_stage.c_str();
        if (ImGui::BeginCombo("display", preview))
        {
            bool const any_selected = current_stage.empty();
            if (ImGui::Selectable("(all)", any_selected) and not any_selected)
            {
                current_stage.clear();
                dirty = true;
            }
            for (auto const& s : graph.stages())
            {
                bool const is_selected = (s.name == current_stage);
                if (ImGui::Selectable(s.name.c_str(), is_selected) and not is_selected)
                {
                    current_stage = s.name;
                    dirty = true;
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        ImGui::TextUnformatted("All stages");

        // Mutating during iteration is awkward; collect the deletion
        // request and apply after the loop.
        std::string to_remove;
        for (auto const& s : graph.stages())
        {
            ImGui::PushID(s.name.c_str());
            ImGui::TextUnformatted(s.name.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
            {
                to_remove = s.name;
            }
            ImGui::PopID();
        }
        if (not to_remove.empty())
        {
            graph.remove_stage(to_remove);
            if (current_stage == to_remove)
            {
                current_stage.clear();
            }
            dirty = true;
        }

        ImGui::Separator();
        static char buf[64] = {0};
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputText("##new_stage", buf, sizeof(buf));
        ImGui::SameLine();
        if (ImGui::Button("Add") and buf[0] != '\0')
        {
            piper::Stage s;
            s.name = buf;
            if (graph.add_stage(s))
            {
                dirty = true;
            }
            buf[0] = '\0';
        }
        return dirty;
    }
}
