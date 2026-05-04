#include "piper/app/panels/stages_panel.h"

#include <cstring>
#include <memory>
#include <string>

#include <imgui.h>

#include "piper/commands.h"
#include "piper/stage.h"

namespace piper::app
{
    bool StagesPanel::draw(piper::Graph&        graph,
                            piper::CommandStack& stack,
                            std::string&         current_stage)
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
            float col[4] = {
                float(s.color.r()) / 255.0f,
                float(s.color.g()) / 255.0f,
                float(s.color.b()) / 255.0f,
                float(s.color.a()) / 255.0f,
            };
            ImGuiColorEditFlags const flags =
                  ImGuiColorEditFlags_NoInputs
                | ImGuiColorEditFlags_NoLabel
                | ImGuiColorEditFlags_AlphaPreview;
            bool const color_changed = ImGui::ColorEdit3("##color", col, flags);
            if (ImGui::IsItemActivated())
            {
                stack.open_group();
                editing_color_ = s.name;
            }
            if (color_changed)
            {
                auto to_byte = [](float x) -> uint8_t
                {
                    if (x <= 0.0f) { return 0; }
                    if (x >= 1.0f) { return 255; }
                    return uint8_t(x * 255.0f + 0.5f);
                };
                rgba const new_c = rgba::from_components(
                    to_byte(col[0]), to_byte(col[1]), to_byte(col[2]),
                    s.color.a());
                stack.push(std::make_unique<SetStageColorCommand>(s.name, new_c),
                           graph);
                dirty = true;
            }
            if (ImGui::IsItemDeactivated() and editing_color_ == s.name)
            {
                stack.close_group();
                editing_color_.clear();
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

        // The arrow buttons are sugar for "move just before / just
        // after the neighbor", which MoveStageCommand handles via
        // its before-snapshot semantics.
        if (not move_up.empty())
        {
            // Move `move_up` to just before its current predecessor.
            std::string predecessor;
            for (std::size_t i = 1; i < graph.stages().size(); ++i)
            {
                if (graph.stages()[i].name == move_up)
                {
                    predecessor = graph.stages()[i - 1].name;
                    break;
                }
            }
            if (not predecessor.empty())
            {
                stack.push(std::make_unique<MoveStageCommand>(move_up, predecessor),
                           graph);
                dirty = true;
            }
        }
        if (not move_down.empty())
        {
            // Move `move_down` so it lands at the slot of its
            // successor (drag-drop semantics).
            std::string successor;
            for (std::size_t i = 0; i + 1 < graph.stages().size(); ++i)
            {
                if (graph.stages()[i].name == move_down)
                {
                    successor = graph.stages()[i + 1].name;
                    break;
                }
            }
            if (not successor.empty())
            {
                stack.push(std::make_unique<MoveStageCommand>(move_down, successor),
                           graph);
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
            stack.push(std::make_unique<MoveStageCommand>(drag_from, target),
                       graph);
            dirty = true;
        }
        if (not to_remove.empty())
        {
            stack.push(std::make_unique<RemoveStageCommand>(to_remove), graph);
            if (current_stage == to_remove)
            {
                current_stage.clear();
            }
            dirty = true;
        }

        ImGui::Separator();
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputText("##new_stage", add_buf_.data(), add_buf_.size());
        ImGui::SameLine();
        if (ImGui::Button("Add") and add_buf_[0] != '\0')
        {
            piper::Stage s;
            s.name = add_buf_.data();
            // Skip if the stage already exists; AddStageCommand
            // would silently no-op but we don't want to bloat undo.
            bool exists = false;
            for (auto const& cur : graph.stages())
            {
                if (cur.name == s.name)
                {
                    exists = true;
                    break;
                }
            }
            if (not exists)
            {
                stack.push(std::make_unique<AddStageCommand>(s), graph);
                dirty = true;
            }
            add_buf_.fill('\0');
        }
        return dirty;
    }
}
