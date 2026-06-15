#include <cfloat>
#include <cstring>
#include <memory>
#include <vector>

#include <imgui.h>

#include "piper/app/panels/modes_panel.h"

#include "piper/commands.h"
#include "piper/mode_profile.h"
#include "piper/registry.h"

namespace piper::studio
{
    bool ModesPanel::draw(piper::Graph&              graph,
                          piper::NodeRegistry const& registry,
                          piper::CommandStack&       stack,
                          piper::Theme const&        theme,
                          std::string&               active_profile)
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

            // Rows are uniform height, so only the visible slice is
            // submitted. An open cell combo closes if its row scrolls
            // out of the clip range.
            ImGuiListClipper clipper;
            clipper.Begin(int(graph.nodes().size()));
            while (clipper.Step())
            {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
                {
                    auto const& node = graph.nodes()[std::size_t(row)];
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

                            auto const advertised = mode_labels_advertised_by(node, registry);
                            if (not advertised.empty())
                            {
                                ImGui::Separator();
                                ImGui::TextDisabled("from this node:");
                                for (auto const& lbl : advertised)
                                {
                                    bool const sel = (current_label == lbl);
                                    if (ImGui::Selectable(lbl.c_str(), sel) and not sel)
                                    {
                                        apply_label(lbl);
                                    }
                                }
                            }

                            bool theme_started = false;
                            for (auto const& kv : theme.mode_colors)
                            {
                                if (kv.first == "enable" or kv.first == "disable")
                                {
                                    continue;
                                }
                                if (not theme_started)
                                {
                                    ImGui::Separator();
                                    ImGui::TextDisabled("from theme:");
                                    theme_started = true;
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
                            ImGui::Separator();
                            ImGui::TextDisabled("custom (Enter to apply):");
                            static char custom_buf[64] = {};
                            ImGui::SetNextItemWidth(-FLT_MIN);
                            if (ImGui::InputText("##custom_label", custom_buf, sizeof(custom_buf),
                                                 ImGuiInputTextFlags_EnterReturnsTrue)
                                and custom_buf[0] != '\0')
                            {
                                apply_label(std::string{ custom_buf });
                                custom_buf[0] = '\0';
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndCombo();
                        }
                        ImGui::PopID();
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
        return dirty;
    }
}
