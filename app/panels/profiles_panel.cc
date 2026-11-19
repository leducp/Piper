#include <cfloat>
#include <memory>
#include <string>

#include <imgui.h>

#include "piper/app/panels/profiles_panel.h"
#include "piper/app/panels/mode_picker.h"

#include "piper/commands.h"
#include "piper/modes.h"
#include "piper/profile.h"

namespace piper::studio
{
    bool ProfilesPanel::draw(piper::Graph&              graph,
                             piper::NodeRegistry const& registry,
                             piper::CommandStack&       stack,
                             std::string&               active_profile)
    {
        bool dirty = false;

        ImGui::SetNextItemWidth(140.0f);
        ImGui::InputText("##new_profile", add_buf_.data(), add_buf_.size());
        ImGui::SameLine();
        if (ImGui::Button("Add profile") and add_buf_[0] != '\0')
        {
            piper::Profile profile;
            profile.name = add_buf_.data();
            bool exists = false;
            for (auto const& cur : graph.profiles())
            {
                if (cur.name == profile.name)
                {
                    exists = true;
                    break;
                }
            }
            if (not exists)
            {
                fill_missing_modes(profile, graph, registry);
                stack.push(std::make_unique<AddProfileCommand>(profile), graph);
                dirty = true;
            }
            add_buf_.fill('\0');
        }
        ImGui::Separator();

        if (graph.profiles().empty())
        {
            ImGui::TextDisabled("(no profile)");
            return dirty;
        }

        std::string to_remove;
        for (auto const& profile : graph.profiles())
        {
            ImGui::PushID(profile.name.c_str());

            bool const is_active = (profile.name == active_profile);
            std::string header = profile.name;
            if (is_active)
            {
                header += "  (active)";
            }
            if (profile.name == graph.default_profile_name())
            {
                header += "  (default)";
            }
            header += "###fold";

            ImGuiTreeNodeFlags header_flags = ImGuiTreeNodeFlags_None;
            if (is_active)
            {
                header_flags = ImGuiTreeNodeFlags_DefaultOpen;
            }
            if (ImGui::CollapsingHeader(header.c_str(), header_flags))
            {
                ImGui::BeginDisabled(is_active);
                if (ImGui::SmallButton("Set active"))
                {
                    active_profile = profile.name;
                    dirty = true;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (profile.name == graph.default_profile_name())
                {
                    if (ImGui::SmallButton("Unset default"))
                    {
                        stack.push(std::make_unique<SetDefaultProfileCommand>(std::string{}), graph);
                        dirty = true;
                    }
                }
                else if (ImGui::SmallButton("Set default"))
                {
                    stack.push(std::make_unique<SetDefaultProfileCommand>(profile.name), graph);
                    dirty = true;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Delete"))
                {
                    to_remove = profile.name;
                }

                ImGuiTableFlags const flags = ImGuiTableFlags_RowBg
                                            | ImGuiTableFlags_BordersInnerV
                                            | ImGuiTableFlags_SizingStretchProp;
                if (ImGui::BeginTable("##modes", 2, flags))
                {
                    ImGui::TableSetupColumn("step", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                    ImGui::TableSetupColumn("mode", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                    for (auto const& node : graph.nodes())
                    {
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextUnformatted(node.name.c_str());
                        ImGui::TableSetColumnIndex(1);

                        std::string current_mode;
                        auto const it = profile.per_node.find(node.id);
                        if (it != profile.per_node.end())
                        {
                            current_mode = it->second;
                        }

                        ImGui::PushID(int(node.id));
                        ImGui::SetNextItemWidth(-FLT_MIN);
                        if (auto picked = mode_combo("##mode", node, registry, current_mode))
                        {
                            stack.push(std::make_unique<SetNodeModeCommand>(
                                           profile.name, node.id, *picked),
                                       graph);
                            dirty = true;
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }
            }
            ImGui::PopID();
        }

        if (not to_remove.empty())
        {
            stack.push(std::make_unique<RemoveProfileCommand>(to_remove), graph);
            if (active_profile == to_remove)
            {
                active_profile.clear();
            }
            dirty = true;
        }
        return dirty;
    }
}
