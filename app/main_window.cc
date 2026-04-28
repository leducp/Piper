#include "piper/app/main_window.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include <imgui.h>

#include "piper/app/theme_loader.h"
#include "piper/builtin_nodes.h"
#include "piper/canvas/event.h"
#include "piper/commands.h"
#include "piper/serialize_v2.h"

namespace piper::app
{
    MainWindow::MainWindow()
        : adapter_(graph_, registry_, theme_)
        , editor_(adapter_)
    {
        register_builtin_nodes(registry_);
        try_load_theme();
        apply_current_theme();
        adapter_.rebuild();

        // Right-click → context menu. On a node: change THAT node's
        // stage (pushes a SetNodeStageCommand so it undoes with the
        // rest). On empty canvas: change the display filter (mirror
        // of the Stages tab).
        editor_.set_context_menu([this](canvas::NodeId hovered, ImVec2 const&)
        {
            if (hovered == canvas::invalid_node_id)
            {
                ImGui::TextDisabled("Display stage");
                ImGui::Separator();
                bool const all_selected = current_stage_.empty();
                if (ImGui::MenuItem("(all)", nullptr, all_selected) and not all_selected)
                {
                    current_stage_.clear();
                    adapter_.set_current_stage(current_stage_);
                    adapter_.rebuild();
                }
                for (auto const& s : graph_.stages())
                {
                    bool const sel = (s.name == current_stage_);
                    if (ImGui::MenuItem(s.name.c_str(), nullptr, sel) and not sel)
                    {
                        current_stage_ = s.name;
                        adapter_.set_current_stage(current_stage_);
                        adapter_.rebuild();
                    }
                }
                return;
            }

            piper::Node const* node = graph_.find_node(NodeId(hovered.v));
            if (node == nullptr)
            {
                return;
            }
            ImGui::Text("Node: %s", node->name.c_str());
            ImGui::Separator();

            if (ImGui::BeginMenu("Set stage"))
            {
                if (graph_.stages().empty())
                {
                    ImGui::TextDisabled("(no stages defined)");
                }
                for (auto const& s : graph_.stages())
                {
                    bool const sel = (s.name == node->stage);
                    if (ImGui::MenuItem(s.name.c_str(), nullptr, sel) and not sel)
                    {
                        command_stack_.push(
                            std::make_unique<SetNodeStageCommand>(NodeId(hovered.v), s.name),
                            graph_);
                        adapter_.rebuild();
                    }
                }
                ImGui::EndMenu();
            }

            // Mode submenu — only if an active profile is selected.
            // Built-in labels first, then any custom labels declared
            // in theme.mode_colors.
            if (not active_mode_profile_.empty())
            {
                std::string current_label;
                for (auto const& mp : graph_.mode_profiles())
                {
                    if (mp.name != active_mode_profile_)
                    {
                        continue;
                    }
                    auto const it = mp.per_node.find(node->id);
                    if (it != mp.per_node.end())
                    {
                        current_label = it->second;
                    }
                    break;
                }

                std::string menu_title = "Set mode (";
                menu_title += active_mode_profile_;
                menu_title += ")";
                if (ImGui::BeginMenu(menu_title.c_str()))
                {
                    auto const apply_label = [&](std::string const& label)
                    {
                        for (auto const& mp : graph_.mode_profiles())
                        {
                            if (mp.name != active_mode_profile_)
                            {
                                continue;
                            }
                            piper::ModeProfile updated = mp;
                            if (label.empty())
                            {
                                updated.per_node.erase(node->id);
                            }
                            else
                            {
                                updated.per_node[node->id] = label;
                            }
                            graph_.remove_mode_profile(mp.name);
                            graph_.add_mode_profile(updated);
                            adapter_.rebuild();
                            break;
                        }
                    };

                    char const* const builtins[] = { "enable", "disable" };
                    for (char const* lbl : builtins)
                    {
                        bool const sel = (current_label == lbl);
                        if (ImGui::MenuItem(lbl, nullptr, sel) and not sel)
                        {
                            apply_label(lbl);
                        }
                    }
                    for (auto const& kv : theme_.mode_colors)
                    {
                        if (kv.first == "enable" or kv.first == "disable")
                        {
                            continue;
                        }
                        bool const sel = (current_label == kv.first);
                        if (ImGui::MenuItem(kv.first.c_str(), nullptr, sel) and not sel)
                        {
                            apply_label(kv.first);
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("(unset)", nullptr, current_label.empty()))
                    {
                        apply_label(std::string{});
                    }
                    ImGui::EndMenu();
                }
            }
        });
    }

    bool MainWindow::load_file(std::string const& path)
    {
        std::ifstream f(path);
        if (not f.is_open())
        {
            std::fprintf(stderr, "could not open %s\n", path.c_str());
            return false;
        }
        std::ostringstream buf;
        buf << f.rdbuf();
        try
        {
            auto result = v2::deserialize(buf.str(), registry_);
            graph_       = std::move(result.graph);
            diagnostics_ = std::move(result.diagnostics);
        }
        catch (std::exception const& e)
        {
            std::fprintf(stderr, "load failed: %s\n", e.what());
            return false;
        }
        loaded_path_ = path;

        // Auto-activate the default mode profile so the inspector
        // and the right-click "Set mode" submenu are immediately
        // useful. Falls back to the first profile if none is flagged
        // as default.
        active_mode_profile_.clear();
        for (auto const& mp : graph_.mode_profiles())
        {
            if (mp.is_default)
            {
                active_mode_profile_ = mp.name;
                break;
            }
        }
        if (active_mode_profile_.empty() and not graph_.mode_profiles().empty())
        {
            active_mode_profile_ = graph_.mode_profiles().front().name;
        }
        adapter_.set_active_mode_profile(active_mode_profile_);
        adapter_.rebuild();

        for (auto const& d : diagnostics_)
        {
            std::fprintf(stderr, "diagnostic: %s\n", d.message.c_str());
        }
        return true;
    }

    void MainWindow::try_load_theme()
    {
        char const* candidates[] = {
            "data/theme.json",
            "../data/theme.json",
            "../../data/theme.json",
        };
        for (char const* p : candidates)
        {
            std::error_code ec;
            if (not std::filesystem::exists(p, ec))
            {
                continue;
            }
            try
            {
                auto result = piper::load_theme(p);
                theme_       = std::move(result.theme);
                theme_path_  = p;
                theme_mtime_ = std::filesystem::last_write_time(p, ec);
                for (auto const& d : result.diagnostics)
                {
                    std::fprintf(stderr, "theme: %s\n", d.message.c_str());
                }
                return;
            }
            catch (std::exception const& e)
            {
                std::fprintf(stderr, "theme load (%s) failed: %s\n", p, e.what());
            }
        }
        std::fprintf(stderr, "theme: no data/theme.json found, using defaults\n");
    }

    void MainWindow::apply_current_theme()
    {
        apply_theme(theme_, canvas_style_, ImGui::GetStyle());
        editor_.set_style(canvas_style_);
    }

    void MainWindow::poll_theme_reload()
    {
        if (theme_path_.empty())
        {
            return;
        }
        auto const now = std::chrono::steady_clock::now();
        if (now - theme_last_check_ < std::chrono::seconds(1))
        {
            return;
        }
        theme_last_check_ = now;

        std::error_code ec;
        auto const mt = std::filesystem::last_write_time(theme_path_, ec);
        if (ec or mt == theme_mtime_)
        {
            return;
        }
        theme_mtime_ = mt;
        try
        {
            auto result = piper::load_theme(theme_path_);
            theme_      = std::move(result.theme);
            apply_current_theme();
            adapter_.rebuild();
            for (auto const& d : result.diagnostics)
            {
                std::fprintf(stderr, "theme reload: %s\n", d.message.c_str());
            }
        }
        catch (std::exception const& e)
        {
            std::fprintf(stderr, "theme reload failed: %s\n", e.what());
        }
    }

    bool MainWindow::draw()
    {
        poll_theme_reload();

        ImGuiViewport const* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("##piper_root",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar     |
                     ImGuiWindowFlags_NoResize       |
                     ImGuiWindowFlags_NoMove         |
                     ImGuiWindowFlags_NoCollapse     |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Quit", "Ctrl+Q"))
                {
                    running_ = false;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                ImGui::MenuItem("About Piper", nullptr, false, false);
                ImGui::EndMenu();
            }
            if (not loaded_path_.empty())
            {
                ImGui::Text("  %s", loaded_path_.c_str());
            }
            ImGui::EndMenuBar();
        }

        // Split: canvas on the left, inspector on the right. No
        // resize splitter yet — fixed inspector width.
        ImVec2 const total = ImGui::GetContentRegionAvail();
        float  const left  = total.x - inspector_width_ - 4.0f;

        ImGui::BeginChild("##canvas_pane", ImVec2{ left, 0 }, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        editor_.draw(ImGui::GetContentRegionAvail());

        for (auto const& ev : editor_.consume_events())
        {
            if (ev.kind == canvas::EventKind::SelectionChanged)
            {
                selection_.clear();
                selection_.reserve(ev.selection.size());
                for (auto const& cid : ev.selection)
                {
                    selection_.push_back(NodeId(cid.v));
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("##right_pane", ImVec2{ inspector_width_, 0 }, true);
        if (ImGui::BeginTabBar("##right_tabs"))
        {
            if (ImGui::BeginTabItem("Inspector"))
            {
                NodeId const selected =
                    selection_.empty() ? invalid_node_id : selection_.front();
                if (inspector_.draw(graph_, command_stack_, selected,
                                    theme_, active_mode_profile_))
                {
                    adapter_.rebuild();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Stages"))
            {
                if (stages_panel_.draw(graph_, current_stage_))
                {
                    adapter_.set_current_stage(current_stage_);
                    adapter_.rebuild();
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Modes"))
            {
                if (modes_panel_.draw(graph_, theme_, active_mode_profile_))
                {
                    adapter_.set_active_mode_profile(active_mode_profile_);
                    adapter_.rebuild();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();

        ImGui::End();

        ImGuiIO const& io = ImGui::GetIO();
        if (io.KeyCtrl and ImGui::IsKeyPressed(ImGuiKey_Q, false))
        {
            running_ = false;
        }
        return running_;
    }
}
