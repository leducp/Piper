#include "piper/app/panels/inspector_panel.h"

#include <cstring>
#include <string>

#include <imgui.h>

#include "piper/attribute.h"
#include "piper/commands.h"
#include "piper/mode_profile.h"

namespace piper::app
{
    bool InspectorPanel::draw(piper::Graph&        graph,
                              piper::CommandStack& stack,
                              NodeId               selected,
                              piper::Theme const&  theme,
                              std::string const&   active_mode_profile)
    {
        ImGui::TextUnformatted("Inspector");
        ImGui::Separator();

        if (selected == invalid_node_id)
        {
            ImGui::TextDisabled("No selection");
            return false;
        }
        Node* node = graph.find_node_mut(selected);
        if (node == nullptr)
        {
            ImGui::TextDisabled("Selected node not found");
            return false;
        }

        bool dirty = false;
        ImGui::PushID(int(selected));

        ImGui::Text("type: %s", node->type.c_str());
        ImGui::Text("id:   %llu", (unsigned long long)node->id);

        // Name — InputText with deferred commit on Enter / focus loss.
        {
            char buf[128];
            std::strncpy(buf, node->name.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("name", buf, sizeof(buf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::string const new_name = buf;
                if (new_name != node->name)
                {
                    stack.push(std::make_unique<RenameNodeCommand>(selected, new_name), graph);
                    dirty = true;
                }
            }
        }

        // Stage — combo if any stages are declared, plain InputText
        // otherwise (free-form). Either path goes through SetNodeStageCommand.
        {
            auto const& stages = graph.stages();
            if (not stages.empty())
            {
                int current = -1;
                for (std::size_t i = 0; i < stages.size(); ++i)
                {
                    if (stages[i].name == node->stage)
                    {
                        current = int(i);
                        break;
                    }
                }
                if (ImGui::BeginCombo("stage",
                                      current >= 0 ? stages[current].name.c_str()
                                                   : node->stage.c_str()))
                {
                    for (std::size_t i = 0; i < stages.size(); ++i)
                    {
                        bool const is_selected = (int(i) == current);
                        if (ImGui::Selectable(stages[i].name.c_str(), is_selected))
                        {
                            if (stages[i].name != node->stage)
                            {
                                stack.push(std::make_unique<SetNodeStageCommand>(
                                               selected, stages[i].name),
                                           graph);
                                dirty = true;
                            }
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            else
            {
                char buf[64];
                std::strncpy(buf, node->stage.c_str(), sizeof(buf) - 1);
                buf[sizeof(buf) - 1] = '\0';
                if (ImGui::InputText("stage", buf, sizeof(buf),
                                     ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    std::string const new_stage = buf;
                    if (new_stage != node->stage)
                    {
                        stack.push(std::make_unique<SetNodeStageCommand>(
                                       selected, new_stage),
                                   graph);
                        dirty = true;
                    }
                }
            }
        }

        // Mode label in the active profile. Direct mutation (no
        // SetModeProfileCommand yet) — see PR 4.7.
        if (not active_mode_profile.empty())
        {
            piper::ModeProfile const* active = nullptr;
            for (auto const& mp : graph.mode_profiles())
            {
                if (mp.name == active_mode_profile)
                {
                    active = &mp;
                    break;
                }
            }
            if (active != nullptr)
            {
                std::string current_label;
                auto const it = active->per_node.find(selected);
                if (it != active->per_node.end())
                {
                    current_label = it->second;
                }

                ImGui::TextDisabled("profile: %s", active_mode_profile.c_str());
                char const* preview = current_label.empty() ? "(unset)"
                                                            : current_label.c_str();
                if (ImGui::BeginCombo("mode", preview))
                {
                    auto const apply_label = [&](std::string const& new_label)
                    {
                        piper::ModeProfile updated = *active;
                        if (new_label.empty())
                        {
                            updated.per_node.erase(selected);
                        }
                        else
                        {
                            updated.per_node[selected] = new_label;
                        }
                        graph.remove_mode_profile(active_mode_profile);
                        graph.add_mode_profile(updated);
                        dirty = true;
                    };

                    char const* const builtins[] = { "enable", "disable" };
                    for (char const* lbl : builtins)
                    {
                        bool const sel = (current_label == lbl);
                        if (ImGui::Selectable(lbl, sel) and not sel)
                        {
                            apply_label(lbl);
                        }
                    }
                    for (auto const& kv : theme.mode_colors)
                    {
                        if (kv.first == "enable" or kv.first == "disable")
                        {
                            continue;
                        }
                        bool const sel = (current_label == kv.first);
                        if (ImGui::Selectable(kv.first.c_str(), sel) and not sel)
                        {
                            apply_label(kv.first);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::Selectable("(unset)", current_label.empty())
                        and not current_label.empty())
                    {
                        apply_label(std::string{});
                    }
                    ImGui::EndCombo();
                }
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Members");

        // Member attributes — only Member role is editable here.
        // Inputs/Outputs are graph topology, not attribute data.
        bool any_member = false;
        for (auto const& attr_const : node->attrs)
        {
            if (attr_const.role != AttributeSpec::Role::Member)
            {
                continue;
            }
            any_member = true;

            ImGui::PushID(attr_const.name.c_str());
            char buf[256];
            std::strncpy(buf, attr_const.value.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText(attr_const.name.c_str(), buf, sizeof(buf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::string const new_val = buf;
                if (new_val != attr_const.value)
                {
                    stack.push(std::make_unique<SetAttributeValueCommand>(
                                   selected, attr_const.name, new_val),
                               graph);
                    dirty = true;
                }
            }
            ImGui::PopID();
        }
        if (not any_member)
        {
            ImGui::TextDisabled("(none)");
        }

        ImGui::PopID();
        return dirty;
    }
}
