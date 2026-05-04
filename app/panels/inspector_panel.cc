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

namespace piper::studio
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

        // Net label: separate edit path. Labels share the NodeId space
        // with nodes; check for a label first since `find_node` returns
        // null for label IDs.
        if (Label* lbl = graph.find_label_mut(selected); lbl != nullptr)
        {
            bool dirty = false;
            ImGui::PushID(int(selected));
            char const* kind_str = "(sink / out)";
            if (lbl->kind == LabelKind::In) { kind_str = "(source / in)"; }
            ImGui::Text("label %s", kind_str);
            ImGui::Text("id: %llu", (unsigned long long)lbl->id);

            char buf[128];
            std::strncpy(buf, lbl->name.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("name", buf, sizeof(buf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::string const new_name{ buf };
                if (new_name != lbl->name)
                {
                    stack.push(std::make_unique<SetLabelNameCommand>(selected, new_name),
                               graph);
                    dirty = true;
                }
            }

            // Cluster color: SetLabelColorCommand fans out to all
            // labels sharing this name so a Source and its Sinks
            // stay visually grouped.
            float col[4] = {
                float(lbl->color.r()) / 255.0f,
                float(lbl->color.g()) / 255.0f,
                float(lbl->color.b()) / 255.0f,
                float(lbl->color.a()) / 255.0f,
            };
            if (ImGui::ColorEdit4("color", col,
                                   ImGuiColorEditFlags_AlphaBar))
            {
                auto to_byte = [](float x) -> uint8_t
                {
                    if (x <= 0.0f) { return 0; }
                    if (x >= 1.0f) { return 255; }
                    return uint8_t(x * 255.0f + 0.5f);
                };
                rgba const new_c = rgba::from_components(
                    to_byte(col[0]), to_byte(col[1]),
                    to_byte(col[2]), to_byte(col[3]));
                if (new_c != lbl->color)
                {
                    stack.push(std::make_unique<SetLabelColorCommand>(selected, new_c),
                               graph);
                    dirty = true;
                }
            }
            ImGui::PopID();
            return dirty;
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
        NodeType const* nt = registry.find(node->type);
        if (nt != nullptr and not nt->help.empty())
        {
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", nt->help.c_str());
            ImGui::PopTextWrapPos();
        }
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

        // Per-instance note: panel-buffered so we can commit on focus loss
        // rather than spamming a command per keystroke. Refreshed when
        // the selection changes; undo/redo refreshes on re-select.
        {
            if (selected != note_buf_node_)
            {
                note_buf_node_ = selected;
                std::strncpy(note_buf_.data(), node->note.c_str(),
                             note_buf_.size() - 1);
                note_buf_.back() = '\0';
            }
            ImVec2 const note_size{ -FLT_MIN, ImGui::GetTextLineHeight() * 4.0f };
            ImGui::InputTextMultiline("##note", note_buf_.data(), note_buf_.size(),
                                       note_size);
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                std::string const new_note{ note_buf_.data() };
                if (new_note != node->note)
                {
                    stack.push(std::make_unique<SetNodeNoteCommand>(selected, new_note), graph);
                    dirty = true;
                }
            }
            ImGui::TextDisabled("note");
        }

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
