#include "piper/app/main_window.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

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

        // Right-click -> context menu. On a node: change THAT node's
        // stage (pushes a SetNodeStageCommand so it undoes with the
        // rest). On empty canvas: change the display filter (mirror
        // of the Stages tab).
        editor_.set_context_menu([this](canvas::NodeId hovered, ImVec2 const& canvas_pos)
        {
            if (hovered == canvas::invalid_node_id)
            {
                if (ImGui::BeginMenu("Add node"))
                {
                    // Group registered types by category. Unspecified
                    // category nodes appear at the top.
                    std::map<std::string, std::vector<piper::NodeType const*>> by_cat;
                    for (auto const* nt : registry_.all())
                    {
                        by_cat[nt->category].push_back(nt);
                    }
                    auto const draw_type_item = [&](piper::NodeType const* nt)
                    {
                        if (ImGui::MenuItem(nt->type.c_str()))
                        {
                            add_node_at(*nt, canvas_pos);
                        }
                    };
                    auto const it_uncat = by_cat.find("");
                    if (it_uncat != by_cat.end())
                    {
                        for (auto const* nt : it_uncat->second)
                        {
                            draw_type_item(nt);
                        }
                        if (by_cat.size() > 1)
                        {
                            ImGui::Separator();
                        }
                    }
                    for (auto const& kv : by_cat)
                    {
                        if (kv.first.empty())
                        {
                            continue;
                        }
                        if (ImGui::BeginMenu(kv.first.c_str()))
                        {
                            for (auto const* nt : kv.second)
                            {
                                draw_type_item(nt);
                            }
                            ImGui::EndMenu();
                        }
                    }
                    ImGui::EndMenu();
                }

                ImGui::Separator();
                ImGui::TextDisabled("Display stage");
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

            // Mode submenu -- only if an active profile is selected.
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
                        graph_.set_node_mode_label(active_mode_profile_, node->id, label);
                        adapter_.rebuild();
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

    void MainWindow::copy_to_clipboard(std::span<canvas::NodeId const> ids)
    {
        clipboard_ = Clipboard{};
        if (ids.empty())
        {
            return;
        }

        std::unordered_set<NodeId> sel;
        sel.reserve(ids.size());
        for (auto const& cid : ids)
        {
            sel.insert(NodeId(cid.v));
        }

        // Find selection origin (top-left).
        Point origin{ std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max() };
        bool any = false;
        for (auto id : sel)
        {
            Node const* n = graph_.find_node(id);
            if (n == nullptr)
            {
                continue;
            }
            any = true;
            if (n->pos.x < origin.x) { origin.x = n->pos.x; }
            if (n->pos.y < origin.y) { origin.y = n->pos.y; }
        }
        if (not any)
        {
            return;
        }
        clipboard_.origin = origin;

        for (auto id : sel)
        {
            Node const* n = graph_.find_node(id);
            if (n == nullptr)
            {
                continue;
            }
            ClipboardEntry e;
            e.node         = *n;
            e.relative_pos = Point{ n->pos.x - origin.x, n->pos.y - origin.y };
            clipboard_.nodes.push_back(std::move(e));
        }
        for (auto const& l : graph_.links())
        {
            if (sel.count(l.from.node) and sel.count(l.to.node))
            {
                clipboard_.internal_links.push_back(l);
            }
        }
    }

    bool MainWindow::paste_from_clipboard(ImVec2 const& at_canvas)
    {
        if (clipboard_.nodes.empty())
        {
            return false;
        }
        std::unordered_map<NodeId, NodeId> id_map;

        for (auto const& e : clipboard_.nodes)
        {
            NodeType const* nt = registry_.find(e.node.type);
            if (nt == nullptr)
            {
                std::fprintf(stderr,
                             "paste: skipping node of unknown type '%s'\n",
                             e.node.type.c_str());
                continue;
            }
            Point const new_pos{ at_canvas.x + e.relative_pos.x,
                                 at_canvas.y + e.relative_pos.y };
            auto cmd = std::make_unique<AddNodeCommand>(
                *nt, e.node.name, e.node.stage, new_pos);
            AddNodeCommand* raw = cmd.get();
            command_stack_.push(std::move(cmd), graph_);
            id_map[e.node.id] = raw->node_id();
            // Pasted nodes get attribute defaults from the registry.
            // Carrying user-edited values across paste needs an
            // InsertNodeCommand with the full Node (not in core
            // commands today). Tracking as a follow-up.
        }

        for (auto const& l : clipboard_.internal_links)
        {
            auto const it_from = id_map.find(l.from.node);
            auto const it_to   = id_map.find(l.to.node);
            if (it_from == id_map.end() or it_to == id_map.end())
            {
                continue;
            }
            PinRef nf{ it_from->second, l.from.attr };
            PinRef nt{ it_to->second,   l.to.attr };
            command_stack_.push(
                std::make_unique<CreateLinkCommand>(nf, nt, l.data_type),
                graph_);
        }
        return true;
    }

    void MainWindow::recompute_lints()
    {
        lint_diagnostics_.clear();

        // Set of (node_id, attr_name) pairs that have at least one
        // link touching them.
        std::set<std::pair<NodeId, std::string>> connected;
        for (auto const& l : graph_.links())
        {
            connected.insert({ l.from.node, l.from.attr });
            connected.insert({ l.to.node,   l.to.attr });
        }

        bool const stages_defined = not graph_.stages().empty();

        piper::ModeProfile const* active_profile = nullptr;
        if (not active_mode_profile_.empty())
        {
            for (auto const& mp : graph_.mode_profiles())
            {
                if (mp.name == active_mode_profile_)
                {
                    active_profile = &mp;
                    break;
                }
            }
        }

        for (auto const& n : graph_.nodes())
        {
            // Stage required when at least one stage is declared.
            if (stages_defined and n.stage.empty())
            {
                Diagnostic d;
                d.kind    = DiagnosticKind::SchemaError;
                d.message = "node '" + n.name + "' has no stage assigned";
                d.node_id = n.id;
                lint_diagnostics_.push_back(d);
            }

            // Disconnected nodes: every Input / Output pin has no
            // link.
            bool any_io        = false;
            bool any_connected = false;
            for (auto const& a : n.attrs)
            {
                if (a.role == AttributeSpec::Role::Member)
                {
                    continue;
                }
                any_io = true;
                if (connected.count({ n.id, a.name }) > 0)
                {
                    any_connected = true;
                    break;
                }
            }
            if (any_io and not any_connected)
            {
                Diagnostic d;
                d.kind    = DiagnosticKind::SchemaError;
                d.message = "node '" + n.name + "' is disconnected";
                d.node_id = n.id;
                lint_diagnostics_.push_back(d);
            }

            // Each Input pin should have at least one source.
            for (auto const& a : n.attrs)
            {
                if (a.role != AttributeSpec::Role::Input)
                {
                    continue;
                }
                if (connected.count({ n.id, a.name }) == 0)
                {
                    Diagnostic d;
                    d.kind      = DiagnosticKind::SchemaError;
                    d.message   = "input '" + n.name + "." + a.name
                                + "' has no source";
                    d.node_id   = n.id;
                    d.attr_name = a.name;
                    lint_diagnostics_.push_back(d);
                }
            }

            // Active-profile coverage: missing nodes default to
            // 'enable' but the user might want to be explicit.
            if (active_profile != nullptr
                and active_profile->per_node.count(n.id) == 0)
            {
                Diagnostic d;
                d.kind    = DiagnosticKind::SchemaError;
                d.message = "node '" + n.name + "' has no entry in profile '"
                          + active_mode_profile_ + "' (treated as 'enable')";
                d.node_id = n.id;
                lint_diagnostics_.push_back(d);
            }
        }
    }

    void MainWindow::add_node_at(piper::NodeType const& type,
                                  ImVec2 const&         canvas_pos)
    {
        // Auto-generate a unique name like "sin_wave", "sin_wave_1", ...
        // so the same type can be dropped repeatedly without manual
        // rename. Cheap O(N*M) lookup; fine for editor sizes.
        std::string base = type.type;
        std::string name = base;
        int counter = 1;
        while (true)
        {
            bool clash = false;
            for (auto const& n : graph_.nodes())
            {
                if (n.name == name)
                {
                    clash = true;
                    break;
                }
            }
            if (not clash)
            {
                break;
            }
            name = base + "_" + std::to_string(counter++);
        }

        Point const pos{ canvas_pos.x, canvas_pos.y };
        command_stack_.push(
            std::make_unique<AddNodeCommand>(type, name, current_stage_, pos),
            graph_);
        adapter_.rebuild();
    }

    void MainWindow::new_document()
    {
        graph_ = piper::Graph{};
        diagnostics_.clear();
        selection_.clear();
        loaded_path_.clear();
        current_stage_.clear();
        active_mode_profile_.clear();
        clipboard_ = Clipboard{};
        command_stack_.clear();
        adapter_.set_current_stage(current_stage_);
        adapter_.set_active_mode_profile(active_mode_profile_);
        adapter_.rebuild();
    }

    bool MainWindow::save_to(std::string const& path)
    {
        if (path.empty())
        {
            return false;
        }
        std::string const json = v2::serialize(graph_);
        std::ofstream f(path);
        if (not f.is_open())
        {
            std::fprintf(stderr, "could not open %s for writing\n", path.c_str());
            return false;
        }
        f << json;
        if (not f)
        {
            std::fprintf(stderr, "write to %s failed\n", path.c_str());
            return false;
        }
        loaded_path_ = path;
        return true;
    }

    bool MainWindow::draw()
    {
        poll_theme_reload();
        recompute_lints();

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
                if (ImGui::MenuItem("New"))
                {
                    new_document();
                }
                if (ImGui::MenuItem("Open..."))
                {
                    path_input_         = loaded_path_;
                    want_open_dialog_   = true;
                }
                bool const save_enabled = not loaded_path_.empty();
                if (ImGui::MenuItem("Save", "Ctrl+S", false, save_enabled))
                {
                    save_to(loaded_path_);
                }
                if (ImGui::MenuItem("Save As..."))
                {
                    path_input_           = loaded_path_;
                    want_save_as_dialog_  = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Quit", "Ctrl+Q"))
                {
                    running_ = false;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View"))
            {
                if (ImGui::MenuItem("Toggle right panel", "Ctrl+B",
                                    inspector_visible_))
                {
                    inspector_visible_ = not inspector_visible_;
                }
                if (ImGui::MenuItem("Snap to grid", "Ctrl+G",
                                    canvas_style_.snap_to_grid))
                {
                    canvas_style_.snap_to_grid = not canvas_style_.snap_to_grid;
                    editor_.set_style(canvas_style_);
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
            std::size_t const total_problems =
                diagnostics_.size() + lint_diagnostics_.size();
            if (total_problems > 0)
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4{1.0f, 0.7f, 0.3f, 1.0f},
                                   "  %zu problem(s)", total_problems);
            }
            ImGui::EndMenuBar();
        }

        // Split: canvas on the left, inspector on the right.
        // Resizable splitter; inspector hides via View > Toggle.
        ImVec2 const total = ImGui::GetContentRegionAvail();
        float  const splitter_w = 6.0f;
        float        right_w    = inspector_width_;
        if (right_w < inspector_min_width_)
        {
            right_w = inspector_min_width_;
        }
        float const max_right = total.x - 100.0f;
        if (right_w > max_right)
        {
            right_w = max_right;
        }
        inspector_width_ = right_w;

        float left = total.x;
        if (inspector_visible_)
        {
            left = total.x - right_w - splitter_w;
        }

        ImGui::BeginChild("##canvas_pane", ImVec2{ left, 0 }, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        editor_.draw(ImGui::GetContentRegionAvail());

        auto const events = editor_.consume_events();

        // Wrap the dispatch in a group so multi-node drag releases
        // and paste-of-N-nodes land on the undo stack as one entry.
        int mutating = 0;
        for (auto const& ev : events)
        {
            switch (ev.kind)
            {
                case canvas::EventKind::NodeMoved:
                case canvas::EventKind::NodeDeleted:
                case canvas::EventKind::LinkCreated:
                case canvas::EventKind::PasteRequested:
                {
                    ++mutating;
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
        bool const group_dispatch = mutating > 1;
        if (group_dispatch)
        {
            command_stack_.open_group();
        }

        bool dirty = false;
        for (auto const& ev : events)
        {
            switch (ev.kind)
            {
                case canvas::EventKind::SelectionChanged:
                {
                    selection_.clear();
                    selection_.reserve(ev.selection.size());
                    for (auto const& cid : ev.selection)
                    {
                        selection_.push_back(NodeId(cid.v));
                    }
                    break;
                }
                case canvas::EventKind::NodeMoved:
                {
                    Point const new_pos{ ev.pos.x, ev.pos.y };
                    command_stack_.push(
                        std::make_unique<MoveNodeCommand>(NodeId(ev.node.v), new_pos),
                        graph_);
                    dirty = true;
                    break;
                }
                case canvas::EventKind::NodeDeleted:
                {
                    command_stack_.push(
                        std::make_unique<DeleteNodeCommand>(NodeId(ev.node.v)),
                        graph_);
                    dirty = true;
                    break;
                }
                case canvas::EventKind::LinkCreated:
                {
                    PinRef const from = adapter_.pin_id_to_ref(ev.pin_from);
                    PinRef const to   = adapter_.pin_id_to_ref(ev.pin_to);
                    if (from.attr.empty() or to.attr.empty())
                    {
                        break;
                    }
                    std::string data_type;
                    Node const* fn = graph_.find_node(from.node);
                    if (fn != nullptr)
                    {
                        Attribute const* a = fn->find_attr(from.attr);
                        if (a != nullptr)
                        {
                            data_type = a->data_type;
                        }
                    }
                    command_stack_.push(
                        std::make_unique<CreateLinkCommand>(from, to, data_type),
                        graph_);
                    dirty = true;
                    break;
                }
                case canvas::EventKind::CopyRequested:
                {
                    copy_to_clipboard(ev.selection);
                    break;
                }
                case canvas::EventKind::PasteRequested:
                {
                    if (paste_from_clipboard(ev.pos))
                    {
                        dirty = true;
                    }
                    break;
                }
                case canvas::EventKind::UndoRequested:
                {
                    if (command_stack_.can_undo())
                    {
                        command_stack_.undo(graph_);
                        dirty = true;
                    }
                    break;
                }
                case canvas::EventKind::RedoRequested:
                {
                    if (command_stack_.can_redo())
                    {
                        command_stack_.redo(graph_);
                        dirty = true;
                    }
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

        if (group_dispatch)
        {
            command_stack_.close_group();
        }
        if (dirty)
        {
            adapter_.rebuild();
        }
        ImGui::EndChild();

        if (inspector_visible_)
        {
            ImGui::SameLine();

            // Splitter: invisible button that consumes drag.
            ImGui::Button("##splitter", ImVec2{ splitter_w, -1.0f });
            if (ImGui::IsItemActive())
            {
                inspector_width_ -= ImGui::GetIO().MouseDelta.x;
            }
            if (ImGui::IsItemHovered() or ImGui::IsItemActive())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }

            ImGui::SameLine();
        }

        if (inspector_visible_)
        {
            ImGui::BeginChild("##right_pane", ImVec2{ inspector_width_, 0 }, true);
        if (ImGui::BeginTabBar("##right_tabs",
                                ImGuiTabBarFlags_FittingPolicyScroll))
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
            std::size_t const tab_problems =
                diagnostics_.size() + lint_diagnostics_.size();
            bool const has_problems = tab_problems > 0;
            if (has_problems)
            {
                // Orange tab BG while there are problems pending.
                ImVec4 const orange{ 0.95f, 0.55f, 0.15f, 1.0f };
                ImGui::PushStyleColor(ImGuiCol_Tab, orange);
                ImGui::PushStyleColor(ImGuiCol_TabHovered, orange);
            }
            char prob_label[32];
            std::snprintf(prob_label, sizeof(prob_label),
                          "Problems (%zu)###problems_tab", tab_problems);
            bool const problems_open = ImGui::BeginTabItem(prob_label);
            if (has_problems)
            {
                ImGui::PopStyleColor(2);
            }
            if (problems_open)
            {
                auto const draw_diag = [&](Diagnostic const& d, int idx)
                {
                    // One-line, fully clickable. Clicking with a
                    // node_id locator selects the affected node so
                    // the user can inspect / fix it.
                    std::string row = "* ";
                    row += d.message;
                    if (d.node_id != invalid_node_id)
                    {
                        row += "  [node ";
                        row += std::to_string(d.node_id);
                        row += "]";
                    }
                    if (not d.attr_name.empty())
                    {
                        row += "  [attr ";
                        row += d.attr_name;
                        row += "]";
                    }
                    if (d.link_id != invalid_link_id)
                    {
                        row += "  [link ";
                        row += std::to_string(d.link_id);
                        row += "]";
                    }

                    ImGui::PushID(idx);
                    if (ImGui::Selectable(row.c_str(), false))
                    {
                        if (d.node_id != invalid_node_id)
                        {
                            selection_.clear();
                            selection_.push_back(d.node_id);
                            canvas::NodeId const cn{ d.node_id };
                            std::array<canvas::NodeId, 1> ids{ cn };
                            editor_.set_selection(ids);
                        }
                    }
                    ImGui::PopID();
                };

                int diag_idx = 0;
                if (not lint_diagnostics_.empty())
                {
                    ImGui::TextUnformatted("Lints");
                    ImGui::Separator();
                    for (auto const& d : lint_diagnostics_)
                    {
                        draw_diag(d, diag_idx++);
                    }
                }
                if (not diagnostics_.empty())
                {
                    if (not lint_diagnostics_.empty())
                    {
                        ImGui::Spacing();
                    }
                    ImGui::TextUnformatted("Load diagnostics");
                    ImGui::Separator();
                    for (auto const& d : diagnostics_)
                    {
                        draw_diag(d, diag_idx++);
                    }
                }
                if (not has_problems)
                {
                    ImGui::TextDisabled("No problems.");
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
            ImGui::EndChild();
        }

        // ----- File modals (popped from the menu) -----
        if (want_open_dialog_)
        {
            ImGui::OpenPopup("Open file");
            want_open_dialog_ = false;
        }
        if (want_save_as_dialog_)
        {
            ImGui::OpenPopup("Save file as");
            want_save_as_dialog_ = false;
        }
        if (ImGui::BeginPopupModal("Open file", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            char buf[512];
            std::strncpy(buf, path_input_.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("path", buf, sizeof(buf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                path_input_ = buf;
                if (load_file(path_input_))
                {
                    ImGui::CloseCurrentPopup();
                }
            }
            else
            {
                path_input_ = buf;
            }
            if (ImGui::Button("Open"))
            {
                if (load_file(path_input_))
                {
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        if (ImGui::BeginPopupModal("Save file as", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize))
        {
            char buf[512];
            std::strncpy(buf, path_input_.c_str(), sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = '\0';
            if (ImGui::InputText("path", buf, sizeof(buf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                path_input_ = buf;
                if (save_to(path_input_))
                {
                    ImGui::CloseCurrentPopup();
                }
            }
            else
            {
                path_input_ = buf;
            }
            if (ImGui::Button("Save"))
            {
                if (save_to(path_input_))
                {
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::End();

        ImGuiIO const& io = ImGui::GetIO();
        if (io.KeyCtrl and ImGui::IsKeyPressed(ImGuiKey_Q, false))
        {
            running_ = false;
        }
        if (io.KeyCtrl and ImGui::IsKeyPressed(ImGuiKey_B, false))
        {
            inspector_visible_ = not inspector_visible_;
        }
        if (io.KeyCtrl and ImGui::IsKeyPressed(ImGuiKey_G, false))
        {
            canvas_style_.snap_to_grid = not canvas_style_.snap_to_grid;
            editor_.set_style(canvas_style_);
        }
        if (io.KeyCtrl and ImGui::IsKeyPressed(ImGuiKey_S, false))
        {
            if (loaded_path_.empty())
            {
                path_input_           = loaded_path_;
                want_save_as_dialog_  = true;
            }
            else
            {
                save_to(loaded_path_);
            }
        }
        return running_;
    }
}
