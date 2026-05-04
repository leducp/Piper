#include "piper/app/panels/inspector_panel.h"

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>

#include "piper/attribute.h"
#include "piper/commands.h"
#include "piper/mode_profile.h"
#include "piper/node_type.h"

namespace piper::app
{
    bool InspectorPanel::draw(piper::Graph&              graph,
                              piper::NodeRegistry const& registry,
                              piper::CommandStack&       stack,
                              NodeId                     selected,
                              piper::Theme const&        theme,
                              std::string const&         active_mode_profile)
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

        // Name -- InputText with deferred commit on Enter / focus loss.
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

        (void)registry;

        ImGui::Separator();
        ImGui::TextUnformatted("Stage");

        {
            char const* preview = "(unset)";
            if (not node->stage.empty())
            {
                preview = node->stage.c_str();
            }
            if (ImGui::BeginCombo("primary", preview))
            {
                bool const unset_sel = node->stage.empty();
                if (ImGui::Selectable("(unset)", unset_sel) and not unset_sel)
                {
                    stack.push(std::make_unique<SetNodeStageCommand>(
                                   selected, std::string{}),
                               graph);
                    dirty = true;
                }
                for (auto const& s : graph.stages())
                {
                    bool const sel = (s.name == node->stage);
                    if (ImGui::Selectable(s.name.c_str(), sel) and not sel)
                    {
                        stack.push(std::make_unique<SetNodeStageCommand>(
                                       selected, s.name),
                                   graph);
                        dirty = true;
                    }
                }
                ImGui::EndCombo();
            }
        }

        bool first_pin = true;
        for (auto const& a : node->attrs)
        {
            if (a.role == AttributeSpec::Role::Member)
            {
                continue;
            }
            if (first_pin)
            {
                ImGui::TextDisabled("Pin overrides");
                first_pin = false;
            }
            ImGui::PushID(a.name.c_str());
            // Multi-element stages: freeze the row to avoid silent truncation.
            if (a.stages.size() > 1)
            {
                ImGui::BeginDisabled();
                ImGui::LabelText(a.name.c_str(), "(multi: %zu stages)", a.stages.size());
                ImGui::EndDisabled();
            }
            else
            {
                std::string current;
                if (not a.stages.empty())
                {
                    current = a.stages.front();
                }
                char const* preview = "(inherit)";
                if (not current.empty())
                {
                    preview = current.c_str();
                }
                if (ImGui::BeginCombo(a.name.c_str(), preview))
                {
                    bool const inherit_sel = current.empty();
                    if (ImGui::Selectable("(inherit)", inherit_sel) and not inherit_sel)
                    {
                        stack.push(std::make_unique<SetAttributeStagesCommand>(
                                       selected, a.name, std::vector<std::string>{}),
                                   graph);
                        dirty = true;
                    }
                    for (auto const& s : graph.stages())
                    {
                        bool const sel = (s.name == current);
                        if (ImGui::Selectable(s.name.c_str(), sel) and not sel)
                        {
                            stack.push(std::make_unique<SetAttributeStagesCommand>(
                                           selected, a.name,
                                           std::vector<std::string>{ s.name }),
                                       graph);
                            dirty = true;
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            ImGui::PopID();
        }


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
                char const* preview = "(unset)";
                if (not current_label.empty())
                {
                    preview = current_label.c_str();
                }
                if (ImGui::BeginCombo("mode", preview))
                {
                    auto const apply_label = [&](std::string const& new_label)
                    {
                        stack.push(std::make_unique<SetNodeModeLabelCommand>(
                                       active_mode_profile, selected, new_label),
                                   graph);
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

        // Member attributes -- only Member role is editable here.
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
