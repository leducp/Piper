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
        ImGui::TextUnformatted("All stages (top -> bottom = engine order)");

        // Collect requests and apply after the loop -- swapping or
        // erasing during iteration would skip / double-act.
        std::string to_remove;
        std::string move_up;
        std::string move_down;
        std::string drag_from;
        std::string drag_before;   // empty = move to end
        bool        drag_to_end = false;

        for (auto const& s : graph.stages())
        {
            ImGui::PushID(s.name.c_str());
            if (ImGui::ArrowButton("##up", ImGuiDir_Up))
            {
                move_up = s.name;
            }
            ImGui::SameLine();
            if (ImGui::ArrowButton("##down", ImGuiDir_Down))
            {
                move_down = s.name;
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
            {
                to_remove = s.name;
            }
            ImGui::SameLine();
            // Selectable is the canonical drag-source widget. Width
            // -FLT_MIN makes it span the rest of the row.
            ImGui::Selectable(s.name.c_str(), false,
                              ImGuiSelectableFlags_AllowOverlap);
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("PIPER_STAGE",
                                          s.name.c_str(),
                                          s.name.size() + 1);
                ImGui::Text("Move %s", s.name.c_str());
                ImGui::EndDragDropSource();
            }
            if (ImGui::BeginDragDropTarget())
            {
                if (auto const* payload = ImGui::AcceptDragDropPayload("PIPER_STAGE"))
                {
                    drag_from   = static_cast<char const*>(payload->Data);
                    drag_before = s.name;
                }
                ImGui::EndDragDropTarget();
            }
            ImGui::PopID();
        }

        // Drop zone past the last row to drop "to the end".
        ImGui::Dummy(ImVec2{ 0.0f, 6.0f });
        if (ImGui::BeginDragDropTarget())
        {
            if (auto const* payload = ImGui::AcceptDragDropPayload("PIPER_STAGE"))
            {
                drag_from   = static_cast<char const*>(payload->Data);
                drag_to_end = true;
            }
            ImGui::EndDragDropTarget();
        }

        if (not move_up.empty())
        {
            if (graph.move_stage_up(move_up))
            {
                dirty = true;
            }
        }
        if (not move_down.empty())
        {
            if (graph.move_stage_down(move_down))
            {
                dirty = true;
            }
        }
        if (not drag_from.empty())
        {
            std::string target;
            if (not drag_to_end)
            {
                target = drag_before;
            }
            if (graph.move_stage_to(drag_from, target))
            {
                dirty = true;
            }
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
