#include "piper/app/panels/modes_panel.h"

#include <cfloat>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <imgui.h>

#include "piper/commands.h"
#include "piper/mode_profile.h"

namespace piper::studio
{
    bool ModesPanel::draw(piper::Graph&        graph,
                          piper::CommandStack& stack,
                          piper::Theme const&  theme,
                          std::string&         active_profile)
    {
        bool dirty = false;

        // ----- Active profile -----
        ImGui::TextUnformatted("Active profile");
        ImGui::Separator();

        char const* active_preview = "(none)";
        if (not active_profile.empty())
        {
            active_preview = active_profile.c_str();
        }
        if (ImGui::BeginCombo("##active", active_preview))
        {
            bool const none_sel = active_profile.empty();
            if (ImGui::Selectable("(none)", none_sel) and not none_sel)
            {
                active_profile.clear();
                dirty = true;
            }
            for (auto const& mp : graph.mode_profiles())
            {
                bool const sel = (mp.name == active_profile);
                if (ImGui::Selectable(mp.name.c_str(), sel) and not sel)
                {
                    active_profile = mp.name;
                    dirty = true;
                }
            }
            ImGui::EndCombo();
        }

        // ----- Profile list / CRUD -----
        ImGui::TextUnformatted("Profiles");
        ImGui::Separator();

        std::string const& dm = graph.default_mode_name();
        std::string to_remove;
        for (auto const& mp : graph.mode_profiles())
        {
            ImGui::PushID(mp.name.c_str());
            ImGui::TextUnformatted(mp.name.c_str());
            if (mp.name == dm)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(default)");
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("x"))
            {
                to_remove = mp.name;
            }
            ImGui::PopID();
        }
        if (not to_remove.empty())
        {
            stack.push(std::make_unique<RemoveModeProfileCommand>(to_remove), graph);
            if (active_profile == to_remove)
            {
                active_profile.clear();
            }
            dirty = true;
        }

        ImGui::Separator();
        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputText("##new_mode", add_buf_.data(), add_buf_.size());
        ImGui::SameLine();
        if (ImGui::Button("Add") and add_buf_[0] != '\0')
        {
            piper::ModeProfile mp;
            mp.name = add_buf_.data();
            bool exists = false;
            for (auto const& cur : graph.mode_profiles())
            {
                if (cur.name == mp.name)
                {
                    exists = true;
                    break;
                }
            }
            if (not exists)
            {
                stack.push(std::make_unique<AddModeProfileCommand>(mp), graph);
                dirty = true;
            }
            add_buf_.fill('\0');
        }

        // ----- Mode labels (built-ins + theme) -----
        ImGui::TextUnformatted("Modes");
        ImGui::Separator();
        ImGui::TextDisabled("enable, disable");
        for (auto const& kv : theme.mode_colors)
        {
            if (kv.first == "enable" or kv.first == "disable")
            {
                continue;
            }
            ImGui::TextDisabled("%s", kv.first.c_str());
        }

        // ----- Matrix: rows = nodes, columns = profiles. Each cell
        // is the node's mode label for that profile. Lets the user
        // fill the meta-mode x node grid in one place. Scrolls if
        // the panel is too narrow.
        ImGui::TextUnformatted("Matrix (profile x node)");
        ImGui::Separator();
        if (graph.mode_profiles().empty() or graph.nodes().empty())
        {
            ImGui::TextDisabled("(add a profile and nodes to fill)");
            return dirty;
        }

        ImGuiTableFlags const flags = ImGuiTableFlags_Borders
                                    | ImGuiTableFlags_RowBg
                                    | ImGuiTableFlags_ScrollX
                                    | ImGuiTableFlags_ScrollY
                                    | ImGuiTableFlags_SizingFixedFit;
        int const cols = int(graph.mode_profiles().size()) + 1;
        ImVec2 const matrix_size{ 0.0f, 220.0f };
        if (ImGui::BeginTable("##mode_matrix", cols, flags, matrix_size))
        {
            ImGui::TableSetupColumn("node", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            for (auto const& mp : graph.mode_profiles())
            {
                ImGui::TableSetupColumn(mp.name.c_str(),
                                         ImGuiTableColumnFlags_WidthFixed, 90.0f);
            }
            ImGui::TableSetupScrollFreeze(1, 1);
            ImGui::TableHeadersRow();

            // Snapshot profile names so per-cell mutation (which goes
            // through remove + re-add) doesn't perturb iteration.
            std::vector<std::string> profile_names;
            profile_names.reserve(graph.mode_profiles().size());
            for (auto const& mp : graph.mode_profiles())
            {
                profile_names.push_back(mp.name);
            }

            for (auto const& node : graph.nodes())
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(node.name.c_str());

                ImGui::PushID(int(node.id));
                int col = 1;
                for (auto const& profile_name : profile_names)
                {
                    ImGui::TableSetColumnIndex(col++);
                    ImGui::PushID(profile_name.c_str());

                    piper::ModeProfile const* current_profile = nullptr;
                    for (auto const& mp : graph.mode_profiles())
                    {
                        if (mp.name == profile_name)
                        {
                            current_profile = &mp;
                            break;
                        }
                    }
                    if (current_profile == nullptr)
                    {
                        ImGui::PopID();
                        continue;
                    }

                    std::string current_label;
                    auto const it = current_profile->per_node.find(node.id);
                    if (it != current_profile->per_node.end())
                    {
                        current_label = it->second;
                    }
                    char const* cell_preview = "(unset)";
                    if (not current_label.empty())
                    {
                        cell_preview = current_label.c_str();
                    }

                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (ImGui::BeginCombo("##cell", cell_preview))
                    {
                        auto const apply_label = [&](std::string const& new_label)
                        {
                            stack.push(std::make_unique<SetNodeModeLabelCommand>(
                                           profile_name, node.id, new_label),
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
                    ImGui::PopID();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        return dirty;
    }
}
