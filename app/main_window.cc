#include "piper/app/main_window.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
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

#include <portable-file-dialogs.h>

#include "piper/app/theme_loader.h"
#include "piper/builtin_nodes.h"
#include "piper/canvas/event.h"
#include "piper/commands.h"
#include "piper/serialize_v2.h"

namespace piper::app
{
    // Native dialogs interpret the second argument as a path-to-open-at.
    // A relative file path like "examples/foo.piper" confuses some
    // platforms' OS dialogs (gtk shows weird URI prefixes). Convert to
    // an absolute parent directory; fall back to cwd when the path is
    // empty or invalid.
    std::string dialog_start_dir(std::string const& reference)
    {
        std::error_code ec;
        if (reference.empty())
        {
            auto cwd = std::filesystem::current_path(ec);
            if (ec)
            {
                return std::string{};
            }
            return cwd.string();
        }
        auto abs = std::filesystem::absolute(reference, ec);
        if (ec)
        {
            return std::string{};
        }
        return abs.parent_path().string();
    }

    MainWindow::MainWindow()
    {
        register_builtin_nodes(registry_);
        try_load_theme();
        apply_current_theme();
    }

    Document* MainWindow::active()
    {
        if (active_doc_idx_ < 0
            or active_doc_idx_ >= int(documents_.size()))
        {
            return nullptr;
        }
        return documents_[active_doc_idx_].get();
    }

    Document const* MainWindow::active() const
    {
        if (active_doc_idx_ < 0
            or active_doc_idx_ >= int(documents_.size()))
        {
            return nullptr;
        }
        return documents_[active_doc_idx_].get();
    }

    std::vector<Diagnostic> const& MainWindow::diagnostics() const
    {
        static std::vector<Diagnostic> const empty{};
        Document const* d = active();
        if (d == nullptr)
        {
            return empty;
        }
        return d->diagnostics;
    }

    Document& MainWindow::add_untitled_document()
    {
        auto doc = std::make_unique<Document>(theme_, registry_);
        wire_document_callbacks(*doc);
        doc->editor.set_style(canvas_style_);
        doc->adapter.rebuild();
        Document& ref = *doc;
        documents_.push_back(std::move(doc));
        active_doc_idx_ = int(documents_.size()) - 1;
        ++next_untitled_id_;
        return ref;
    }

    void MainWindow::wire_document_callbacks(Document& doc)
    {
        Document* dp = &doc;
        doc.editor.set_context_menu([this, dp](canvas::NodeId hovered, ImVec2 const& canvas_pos)
        {
            if (hovered == canvas::invalid_node_id)
            {
                if (ImGui::BeginMenu("Add node"))
                {
                    std::map<std::string, std::vector<piper::NodeType const*>> by_cat;
                    for (auto const* nt : registry_.all())
                    {
                        by_cat[nt->category].push_back(nt);
                    }
                    auto const draw_type_item = [&](piper::NodeType const* nt)
                    {
                        if (ImGui::MenuItem(nt->type.c_str()))
                        {
                            add_node_at(*dp, *nt, canvas_pos);
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
                bool const all_selected = dp->current_stage.empty();
                if (ImGui::MenuItem("(all)", nullptr, all_selected) and not all_selected)
                {
                    dp->current_stage.clear();
                    dp->adapter.set_current_stage(dp->current_stage);
                    dp->adapter.rebuild();
                }
                for (auto const& s : dp->graph.stages())
                {
                    bool const sel = (s.name == dp->current_stage);
                    if (ImGui::MenuItem(s.name.c_str(), nullptr, sel) and not sel)
                    {
                        dp->current_stage = s.name;
                        dp->adapter.set_current_stage(dp->current_stage);
                        dp->adapter.rebuild();
                    }
                }
                return;
            }

            piper::Node const* node = dp->graph.find_node(NodeId(hovered.v));
            if (node == nullptr)
            {
                return;
            }
            ImGui::Text("Node: %s", node->name.c_str());
            ImGui::Separator();

            if (ImGui::BeginMenu("Set stage"))
            {
                if (dp->graph.stages().empty())
                {
                    ImGui::TextDisabled("(no stages defined)");
                }
                else
                {
                    piper::NodeType const* nt = registry_.find(node->type);
                    if (nt == nullptr)
                    {
                        ImGui::TextDisabled("(unknown type)");
                    }
                    else
                    {
                        std::vector<std::string> slots = nt->slots;
                        if (slots.empty())
                        {
                            slots.push_back("tick");
                        }
                        for (auto const& slot : slots)
                        {
                            std::string current;
                            auto const it = node->slot_bindings.find(slot);
                            if (it != node->slot_bindings.end())
                            {
                                current = it->second;
                            }
                            if (ImGui::BeginMenu(slot.c_str()))
                            {
                                bool const none_sel = current.empty();
                                if (ImGui::MenuItem("(unbound)", nullptr, none_sel)
                                    and not none_sel)
                                {
                                    dp->command_stack.push(
                                        std::make_unique<BindSlotCommand>(
                                            node->id, slot, std::string{}),
                                        dp->graph);
                                    dp->dirty = true;
                                    dp->adapter.rebuild();
                                }
                                for (auto const& s : dp->graph.stages())
                                {
                                    bool const sel = (s.name == current);
                                    if (ImGui::MenuItem(s.name.c_str(), nullptr, sel)
                                        and not sel)
                                    {
                                        dp->command_stack.push(
                                            std::make_unique<BindSlotCommand>(
                                                node->id, slot, s.name),
                                            dp->graph);
                                        dp->dirty = true;
                                        dp->adapter.rebuild();
                                    }
                                }
                                ImGui::EndMenu();
                            }
                        }
                    }
                }
                ImGui::EndMenu();
            }

            if (not dp->active_mode_profile.empty())
            {
                std::string current_label;
                for (auto const& mp : dp->graph.mode_profiles())
                {
                    if (mp.name != dp->active_mode_profile)
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
                menu_title += dp->active_mode_profile;
                menu_title += ")";
                if (ImGui::BeginMenu(menu_title.c_str()))
                {
                    auto const apply_label = [&](std::string const& label)
                    {
                        dp->command_stack.push(
                            std::make_unique<SetNodeModeLabelCommand>(
                                dp->active_mode_profile, node->id, label),
                            dp->graph);
                        dp->dirty = true;
                        dp->adapter.rebuild();
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
        piper::v2::BundleLoadResult bundle;
        try
        {
            bundle = v2::deserialize_bundle(buf.str(), registry_);
        }
        catch (std::exception const& e)
        {
            std::fprintf(stderr, "load failed: %s\n", e.what());
            return false;
        }

        if (bundle.pipelines.empty())
        {
            std::fprintf(stderr, "load: %s contained no pipelines\n", path.c_str());
            for (auto const& d : bundle.diagnostics)
            {
                std::fprintf(stderr, "diagnostic: %s\n", d.message.c_str());
            }
            return false;
        }

        // The first pipeline reuses the active untitled doc (so opening
        // a file on startup doesn't leave an empty tab). Extra pipelines
        // always open as new tabs.
        bool first = true;
        for (auto& p : bundle.pipelines)
        {
            Document* target = nullptr;
            if (first)
            {
                Document* a = active();
                bool const reusable = (a != nullptr
                                       and a->loaded_path.empty()
                                       and a->graph.nodes().empty()
                                       and not a->dirty);
                if (reusable)
                {
                    target = a;
                }
                first = false;
            }
            if (target == nullptr)
            {
                target = &add_untitled_document();
            }

            target->graph         = std::move(p.graph);
            target->diagnostics   = std::move(p.diagnostics);
            target->loaded_path   = path;
            target->pipeline_name = std::move(p.name);
            target->dirty         = false;
            target->command_stack.clear();
            target->selection.clear();
            target->current_stage.clear();
            target->active_mode_profile.clear();

            std::string const& dm = target->graph.default_mode_name();
            if (not dm.empty())
            {
                for (auto const& mp : target->graph.mode_profiles())
                {
                    if (mp.name == dm)
                    {
                        target->active_mode_profile = dm;
                        break;
                    }
                }
            }
            if (target->active_mode_profile.empty()
                and not target->graph.mode_profiles().empty())
            {
                target->active_mode_profile = target->graph.mode_profiles().front().name;
            }
            target->adapter.set_current_stage(target->current_stage);
            target->adapter.set_active_mode_profile(target->active_mode_profile);
            target->adapter.rebuild();

            for (auto const& d : target->diagnostics)
            {
                std::fprintf(stderr, "diagnostic: %s\n", d.message.c_str());
            }
        }
        for (auto const& d : bundle.diagnostics)
        {
            std::fprintf(stderr, "bundle diagnostic: %s\n", d.message.c_str());
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
        for (auto& doc : documents_)
        {
            doc->editor.set_style(canvas_style_);
        }
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
            for (auto& doc : documents_)
            {
                doc->adapter.rebuild();
            }
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

    void MainWindow::copy_to_clipboard(Document& doc, std::span<canvas::NodeId const> ids)
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

        Point origin{ std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max() };
        bool any = false;
        for (auto id : sel)
        {
            Node const* n = doc.graph.find_node(id);
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
            Node const* n = doc.graph.find_node(id);
            if (n == nullptr)
            {
                continue;
            }
            ClipboardEntry e;
            e.node         = *n;
            e.relative_pos = Point{ n->pos.x - origin.x, n->pos.y - origin.y };
            clipboard_.nodes.push_back(std::move(e));
        }
        for (auto const& l : doc.graph.links())
        {
            if (sel.count(l.from.node) and sel.count(l.to.node))
            {
                clipboard_.internal_links.push_back(l);
            }
        }
    }

    bool MainWindow::paste_from_clipboard(Document& doc, ImVec2 const& at_canvas)
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
                *nt, e.node.name, new_pos);
            AddNodeCommand* raw = cmd.get();
            doc.command_stack.push(std::move(cmd), doc.graph);
            NodeId const new_id = raw->node_id();
            id_map[e.node.id] = new_id;
            for (auto const& [slot, stage] : e.node.slot_bindings)
            {
                doc.command_stack.push(
                    std::make_unique<BindSlotCommand>(new_id, slot, stage),
                    doc.graph);
            }
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
            doc.command_stack.push(
                std::make_unique<CreateLinkCommand>(nf, nt, l.data_type),
                doc.graph);
        }
        doc.dirty = true;
        return true;
    }

    void MainWindow::goto_next_stage(Document& doc)
    {
        auto const& stages = doc.graph.stages();
        if (stages.empty())
        {
            return;
        }
        int idx = -1;
        for (std::size_t i = 0; i < stages.size(); ++i)
        {
            if (stages[i].name == doc.current_stage)
            {
                idx = int(i);
                break;
            }
        }
        std::size_t next_idx = 0;
        if (idx >= 0)
        {
            next_idx = (std::size_t(idx) + 1) % stages.size();
        }
        doc.current_stage = stages[next_idx].name;
        doc.adapter.set_current_stage(doc.current_stage);
        doc.adapter.rebuild();
    }

    void MainWindow::goto_prev_stage(Document& doc)
    {
        auto const& stages = doc.graph.stages();
        if (stages.empty())
        {
            return;
        }
        int idx = -1;
        for (std::size_t i = 0; i < stages.size(); ++i)
        {
            if (stages[i].name == doc.current_stage)
            {
                idx = int(i);
                break;
            }
        }
        std::size_t prev_idx = stages.size() - 1;
        if (idx > 0)
        {
            prev_idx = std::size_t(idx) - 1;
        }
        doc.current_stage = stages[prev_idx].name;
        doc.adapter.set_current_stage(doc.current_stage);
        doc.adapter.rebuild();
    }

    void MainWindow::toggle_stage_play()
    {
        stage_play_active_ = not stage_play_active_;
        if (stage_play_active_)
        {
            stage_play_next_advance_ =
                std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
        }
    }

    void MainWindow::tick_stage_play(Document& doc)
    {
        if (not stage_play_active_)
        {
            return;
        }
        auto const now = std::chrono::steady_clock::now();
        if (now < stage_play_next_advance_)
        {
            return;
        }
        goto_next_stage(doc);
        stage_play_next_advance_ = now + std::chrono::milliseconds(2000);
    }

    void MainWindow::recompute_lints(Document& doc)
    {
        doc.lint_diagnostics.clear();

        std::set<std::pair<NodeId, std::string>> connected;
        for (auto const& l : doc.graph.links())
        {
            connected.insert({ l.from.node, l.from.attr });
            connected.insert({ l.to.node,   l.to.attr });
        }

        bool const stages_defined = not doc.graph.stages().empty();

        piper::ModeProfile const* active_profile = nullptr;
        if (not doc.active_mode_profile.empty())
        {
            for (auto const& mp : doc.graph.mode_profiles())
            {
                if (mp.name == doc.active_mode_profile)
                {
                    active_profile = &mp;
                    break;
                }
            }
        }

        for (auto const& n : doc.graph.nodes())
        {
            if (stages_defined and n.slot_bindings.empty())
            {
                Diagnostic d;
                d.event   = Diagnostic::Event::SchemaError;
                d.message = "node '" + n.name + "' has no slot bound to any stage";
                d.node_id = n.id;
                doc.lint_diagnostics.push_back(d);
            }

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
                d.event   = Diagnostic::Event::SchemaError;
                d.message = "node '" + n.name + "' is disconnected";
                d.node_id = n.id;
                doc.lint_diagnostics.push_back(d);
            }

            for (auto const& a : n.attrs)
            {
                if (a.role != AttributeSpec::Role::Input)
                {
                    continue;
                }
                if (connected.count({ n.id, a.name }) == 0)
                {
                    Diagnostic d;
                    d.event     = Diagnostic::Event::SchemaError;
                    d.message   = "input '" + n.name + "." + a.name
                                + "' has no source";
                    d.node_id   = n.id;
                    d.attr_name = a.name;
                    doc.lint_diagnostics.push_back(d);
                }
            }

            if (active_profile != nullptr
                and active_profile->per_node.count(n.id) == 0)
            {
                Diagnostic d;
                d.event   = Diagnostic::Event::SchemaError;
                d.message = "node '" + n.name + "' has no entry in profile '"
                          + doc.active_mode_profile + "' (treated as 'enable')";
                d.node_id = n.id;
                doc.lint_diagnostics.push_back(d);
            }
        }
    }

    void MainWindow::add_node_at(Document& doc,
                                  piper::NodeType const& type,
                                  ImVec2 const&         canvas_pos)
    {
        std::string base = type.type;
        std::string name = base;
        int counter = 1;
        while (true)
        {
            bool clash = false;
            for (auto const& n : doc.graph.nodes())
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
        auto add_cmd = std::make_unique<AddNodeCommand>(type, name, pos);
        AddNodeCommand* raw = add_cmd.get();
        doc.command_stack.push(std::move(add_cmd), doc.graph);
        if (not doc.current_stage.empty())
        {
            // TODO: bind to all of the type's slots once slot UI lands;
            // for now bind the canonical "tick" slot.
            doc.command_stack.push(
                std::make_unique<BindSlotCommand>(raw->node_id(), "tick", doc.current_stage),
                doc.graph);
        }
        doc.dirty = true;
        doc.adapter.rebuild();
    }

    bool MainWindow::save_to(Document& doc, std::string const& path)
    {
        if (path.empty())
        {
            return false;
        }

        // Gather every other tab whose loaded_path matches `path`. This
        // preserves the bundled structure when one tab from a multi-
        // pipeline file is saved -- its siblings remain in the file.
        // `doc` is always written first so it lands at index 0 unless
        // it shares the path with siblings already on disk in some
        // other order (their order is preserved).
        std::vector<v2::PipelineRef> refs;
        refs.reserve(documents_.size());
        refs.push_back({ doc.pipeline_name, &doc.graph });
        for (auto& other : documents_)
        {
            if (other.get() == &doc)
            {
                continue;
            }
            if (other->loaded_path == path and not path.empty())
            {
                refs.push_back({ other->pipeline_name, &other->graph });
            }
        }

        std::string const json = v2::serialize_bundle(refs);
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
        doc.loaded_path = path;
        doc.dirty       = false;
        // Sibling tabs that contributed to the bundle also become clean
        // (their on-disk content matches their in-memory graph again).
        for (auto& other : documents_)
        {
            if (other.get() == &doc)
            {
                continue;
            }
            if (other->loaded_path == path and not path.empty())
            {
                other->dirty = false;
            }
        }
        return true;
    }

    std::string MainWindow::tab_title(Document const& doc, int idx) const
    {
        std::string title;
        if (doc.loaded_path.empty())
        {
            title = "untitled-" + std::to_string(idx + 1);
            if (not doc.pipeline_name.empty())
            {
                title += ": ";
                title += doc.pipeline_name;
            }
        }
        else
        {
            title = std::filesystem::path(doc.loaded_path).filename().string();
            if (title.empty())
            {
                title = doc.loaded_path;
            }
            // Append the pipeline name when this file holds more than
            // one pipeline (so the user can distinguish siblings).
            if (not doc.pipeline_name.empty())
            {
                int siblings = 0;
                for (auto const& other : documents_)
                {
                    if (other->loaded_path == doc.loaded_path)
                    {
                        ++siblings;
                    }
                }
                if (siblings > 1)
                {
                    title += " : ";
                    title += doc.pipeline_name;
                }
            }
        }
        if (doc.dirty)
        {
            title += "*";
        }
        // Append an ImGui-stable id so two tabs with the same filename
        // (siblings in different directories) do not collide.
        title += "###doc";
        title += std::to_string(idx);
        return title;
    }

    void MainWindow::request_close(int idx)
    {
        if (idx < 0 or idx >= int(documents_.size()))
        {
            return;
        }
        if (documents_[idx]->dirty)
        {
            confirm_close_idx_ = idx;
            ImGui::OpenPopup("##confirm_close");
            return;
        }
        pending_close_idx_ = idx;
    }

    void MainWindow::process_pending_close()
    {
        if (pending_close_idx_ < 0
            or pending_close_idx_ >= int(documents_.size()))
        {
            pending_close_idx_ = -1;
            return;
        }
        documents_.erase(documents_.begin() + pending_close_idx_);
        if (active_doc_idx_ >= int(documents_.size()))
        {
            active_doc_idx_ = int(documents_.size()) - 1;
        }
        if (active_doc_idx_ < 0 and not documents_.empty())
        {
            active_doc_idx_ = 0;
        }
        pending_close_idx_ = -1;
    }

    bool MainWindow::draw()
    {
        if (documents_.empty())
        {
            add_untitled_document();
        }

        Document& doc = *active();

        poll_theme_reload();
        tick_stage_play(doc);
        recompute_lints(doc);

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
                if (ImGui::MenuItem("New", "Ctrl+T"))
                {
                    add_untitled_document();
                }
                if (ImGui::MenuItem("Open..."))
                {
                    auto picked = pfd::open_file(
                        "Open Piper file",
                        dialog_start_dir(doc.loaded_path),
                        { "Piper graphs", "*.piper *.json", "All files", "*" }).result();
                    if (not picked.empty())
                    {
                        load_file(picked.front());
                    }
                }
                bool const save_enabled = not doc.loaded_path.empty();
                if (ImGui::MenuItem("Save", "Ctrl+S", false, save_enabled))
                {
                    save_to(doc, doc.loaded_path);
                }
                if (ImGui::MenuItem("Save As..."))
                {
                    auto picked = pfd::save_file(
                        "Save Piper file as",
                        dialog_start_dir(doc.loaded_path),
                        { "Piper graphs", "*.piper", "All files", "*" }).result();
                    if (not picked.empty())
                    {
                        save_to(doc, picked);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Close tab", "Ctrl+W",
                                    false, not documents_.empty()))
                {
                    request_close(active_doc_idx_);
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
                    for (auto& d : documents_)
                    {
                        d->editor.set_style(canvas_style_);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                ImGui::MenuItem("About Piper", nullptr, false, false);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // ----- Document tabs -----
        {
            ImGuiTabBarFlags const flags = ImGuiTabBarFlags_Reorderable
                                          | ImGuiTabBarFlags_FittingPolicyScroll
                                          | ImGuiTabBarFlags_TabListPopupButton;
            if (ImGui::BeginTabBar("##doc_tabs", flags))
            {
                int requested_close = -1;
                int new_active      = active_doc_idx_;
                for (int i = 0; i < int(documents_.size()); ++i)
                {
                    bool open = true;
                    std::string title = tab_title(*documents_[i], i);
                    if (ImGui::BeginTabItem(title.c_str(), &open,
                                            ImGuiTabItemFlags_None))
                    {
                        new_active = i;
                        ImGui::EndTabItem();
                    }
                    if (not open)
                    {
                        requested_close = i;
                    }
                }
                ImGui::EndTabBar();
                if (new_active != active_doc_idx_)
                {
                    active_doc_idx_ = new_active;
                }
                if (requested_close >= 0)
                {
                    request_close(requested_close);
                }
            }

            // Confirmation popup for closing a dirty document.
            if (ImGui::BeginPopupModal("##confirm_close", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                if (confirm_close_idx_ >= 0
                    and confirm_close_idx_ < int(documents_.size()))
                {
                    Document& cd = *documents_[confirm_close_idx_];
                    std::string label = cd.loaded_path.empty()
                                            ? std::string{"untitled"}
                                            : std::filesystem::path(cd.loaded_path).filename().string();
                    ImGui::Text("'%s' has unsaved changes.", label.c_str());
                    ImGui::Separator();
                    if (ImGui::Button("Save"))
                    {
                        if (cd.loaded_path.empty())
                        {
                            auto picked = pfd::save_file(
                                "Save Piper file as",
                                dialog_start_dir(cd.loaded_path),
                                { "Piper graphs", "*.piper", "All files", "*" }).result();
                            if (not picked.empty() and save_to(cd, picked))
                            {
                                pending_close_idx_ = confirm_close_idx_;
                            }
                        }
                        else if (save_to(cd, cd.loaded_path))
                        {
                            pending_close_idx_ = confirm_close_idx_;
                        }
                        confirm_close_idx_ = -1;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Discard"))
                    {
                        pending_close_idx_ = confirm_close_idx_;
                        confirm_close_idx_ = -1;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel"))
                    {
                        confirm_close_idx_ = -1;
                        ImGui::CloseCurrentPopup();
                    }
                }
                else
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        // The active document may have changed via the tab bar; re-bind.
        Document& adoc = *active();

        // ----- Toolbar row: stage + profile pickers -----
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(" stage");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        char const* stage_preview = "(all)";
        if (not adoc.current_stage.empty())
        {
            stage_preview = adoc.current_stage.c_str();
        }
        if (ImGui::BeginCombo("##tb_stage", stage_preview))
        {
            if (ImGui::Selectable("(all)", adoc.current_stage.empty()))
            {
                adoc.current_stage.clear();
                adoc.adapter.set_current_stage(adoc.current_stage);
                adoc.adapter.rebuild();
            }
            for (auto const& s : adoc.graph.stages())
            {
                bool const sel = (s.name == adoc.current_stage);
                if (ImGui::Selectable(s.name.c_str(), sel) and not sel)
                {
                    adoc.current_stage = s.name;
                    adoc.adapter.set_current_stage(adoc.current_stage);
                    adoc.adapter.rebuild();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        if (ImGui::Button("<##tb_stage_prev"))
        {
            goto_prev_stage(adoc);
        }
        ImGui::SameLine();
        char const* tb_play_label = "play##tb_play";
        if (stage_play_active_)
        {
            tb_play_label = "stop##tb_play";
        }
        if (ImGui::Button(tb_play_label))
        {
            toggle_stage_play();
        }
        ImGui::SameLine();
        if (ImGui::Button(">##tb_stage_next"))
        {
            goto_next_stage(adoc);
        }

        ImGui::SameLine();
        ImGui::Dummy(ImVec2{ 12.0f, 0.0f });
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("profile");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(140.0f);
        char const* profile_preview = "(none)";
        if (not adoc.active_mode_profile.empty())
        {
            profile_preview = adoc.active_mode_profile.c_str();
        }
        if (ImGui::BeginCombo("##tb_profile", profile_preview))
        {
            if (ImGui::Selectable("(none)", adoc.active_mode_profile.empty()))
            {
                adoc.active_mode_profile.clear();
                adoc.adapter.set_active_mode_profile(adoc.active_mode_profile);
                adoc.adapter.rebuild();
            }
            for (auto const& mp : adoc.graph.mode_profiles())
            {
                bool const sel = (mp.name == adoc.active_mode_profile);
                if (ImGui::Selectable(mp.name.c_str(), sel) and not sel)
                {
                    adoc.active_mode_profile = mp.name;
                    adoc.adapter.set_active_mode_profile(adoc.active_mode_profile);
                    adoc.adapter.rebuild();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::Separator();

        // ----- Workspace: canvas + inspector for active doc -----
        ImVec2 const total       = ImGui::GetContentRegionAvail();
        float  const status_h    = ImGui::GetFrameHeightWithSpacing();
        float  const content_h   = total.y - status_h;
        float  const splitter_w  = 6.0f;
        float        right_w     = inspector_width_;
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

        ImGui::BeginChild("##canvas_pane", ImVec2{ left, content_h }, false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        adoc.editor.draw(ImGui::GetContentRegionAvail());
        process_editor_events(adoc);
        ImGui::EndChild();

        if (inspector_visible_)
        {
            ImGui::SameLine();
            ImGui::Button("##splitter", ImVec2{ splitter_w, content_h });
            if (ImGui::IsItemActive())
            {
                inspector_width_ -= ImGui::GetIO().MouseDelta.x;
            }
            if (ImGui::IsItemHovered() or ImGui::IsItemActive())
            {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            }
            ImGui::SameLine();

            ImGui::BeginChild("##right_pane", ImVec2{ inspector_width_, content_h }, true);
            if (ImGui::BeginTabBar("##right_tabs",
                                    ImGuiTabBarFlags_FittingPolicyScroll))
            {
                if (ImGui::BeginTabItem("Inspector"))
                {
                    NodeId const selected =
                        adoc.selection.empty() ? invalid_node_id : adoc.selection.front();
                    if (inspector_.draw(adoc.graph, registry_, adoc.command_stack, selected,
                                        theme_, adoc.active_mode_profile))
                    {
                        adoc.dirty = true;
                        adoc.adapter.rebuild();
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Stages"))
                {
                    if (stages_panel_.draw(adoc.graph, adoc.command_stack, adoc.current_stage))
                    {
                        adoc.dirty = true;
                        adoc.adapter.set_current_stage(adoc.current_stage);
                        adoc.adapter.rebuild();
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Modes"))
                {
                    if (modes_panel_.draw(adoc.graph, adoc.command_stack, theme_, adoc.active_mode_profile))
                    {
                        adoc.dirty = true;
                        adoc.adapter.set_active_mode_profile(adoc.active_mode_profile);
                        adoc.adapter.rebuild();
                    }
                    ImGui::EndTabItem();
                }
                std::size_t const tab_problems =
                    adoc.diagnostics.size() + adoc.lint_diagnostics.size();
                bool const has_problems = tab_problems > 0;
                if (has_problems)
                {
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
                                adoc.selection.clear();
                                adoc.selection.push_back(d.node_id);
                                canvas::NodeId const cn{ d.node_id };
                                std::array<canvas::NodeId, 1> ids{ cn };
                                adoc.editor.set_selection(ids);
                                adoc.editor.scroll_to(cn);
                            }
                        }
                        ImGui::PopID();
                    };

                    int diag_idx = 0;
                    if (not adoc.lint_diagnostics.empty())
                    {
                        ImGui::TextUnformatted("Lints");
                        ImGui::Separator();
                        for (auto const& d : adoc.lint_diagnostics)
                        {
                            draw_diag(d, diag_idx++);
                        }
                    }
                    if (not adoc.diagnostics.empty())
                    {
                        if (not adoc.lint_diagnostics.empty())
                        {
                            ImGui::Spacing();
                        }
                        ImGui::TextUnformatted("Load diagnostics");
                        ImGui::Separator();
                        for (auto const& d : adoc.diagnostics)
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

        // ----- Status bar: path + problem count -----
        ImGui::Separator();
        ImGui::AlignTextToFramePadding();
        if (not adoc.loaded_path.empty())
        {
            ImGui::TextDisabled("%s", adoc.loaded_path.c_str());
        }
        else
        {
            ImGui::TextDisabled("(unsaved)");
        }
        ImGui::SameLine();
        std::size_t const total_problems =
            adoc.diagnostics.size() + adoc.lint_diagnostics.size();
        if (total_problems > 0)
        {
            ImGui::TextColored(ImVec4{1.0f, 0.7f, 0.3f, 1.0f},
                               "  %zu problem(s)", total_problems);
        }
        else
        {
            ImGui::TextDisabled("  no problems");
        }

        ImGui::End();

        // App-level shortcuts. Each is claimed with RouteAlways so
        // ImGui built-ins (menu nav on Alt, word nav on Ctrl, list
        // nav on PageUp/Down, ...) cannot fire alongside us.
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Q,
                            ImGuiInputFlags_RouteAlways))
        {
            running_ = false;
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_T,
                            ImGuiInputFlags_RouteAlways))
        {
            add_untitled_document();
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_W,
                            ImGuiInputFlags_RouteAlways))
        {
            request_close(active_doc_idx_);
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Tab,
                            ImGuiInputFlags_RouteAlways))
        {
            if (not documents_.empty())
            {
                active_doc_idx_ = (active_doc_idx_ + 1) % int(documents_.size());
            }
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_B,
                            ImGuiInputFlags_RouteAlways))
        {
            inspector_visible_ = not inspector_visible_;
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_G,
                            ImGuiInputFlags_RouteAlways))
        {
            canvas_style_.snap_to_grid = not canvas_style_.snap_to_grid;
            for (auto& d : documents_)
            {
                d->editor.set_style(canvas_style_);
            }
        }
        if (ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_LeftArrow,
                            ImGuiInputFlags_RouteAlways))
        {
            goto_prev_stage(adoc);
        }
        if (ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_RightArrow,
                            ImGuiInputFlags_RouteAlways))
        {
            goto_next_stage(adoc);
        }
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S,
                            ImGuiInputFlags_RouteAlways))
        {
            if (adoc.loaded_path.empty())
            {
                auto picked = pfd::save_file(
                    "Save Piper file as",
                    dialog_start_dir(adoc.loaded_path),
                    { "Piper graphs", "*.piper", "All files", "*" }).result();
                if (not picked.empty())
                {
                    save_to(adoc, picked);
                }
            }
            else
            {
                save_to(adoc, adoc.loaded_path);
            }
        }

        process_pending_close();
        return running_;
    }

    void MainWindow::process_editor_events(Document& doc)
    {
        auto const events = doc.editor.consume_events();

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
            doc.command_stack.open_group();
        }

        bool dirty_rebuild = false;
        for (auto const& ev : events)
        {
            switch (ev.kind)
            {
                case canvas::EventKind::SelectionChanged:
                {
                    doc.selection.clear();
                    doc.selection.reserve(ev.selection.size());
                    for (auto const& cid : ev.selection)
                    {
                        doc.selection.push_back(NodeId(cid.v));
                    }
                    break;
                }
                case canvas::EventKind::NodeMoved:
                {
                    Point const new_pos{ ev.pos.x, ev.pos.y };
                    doc.command_stack.push(
                        std::make_unique<MoveNodeCommand>(NodeId(ev.node.v), new_pos),
                        doc.graph);
                    doc.dirty     = true;
                    dirty_rebuild = true;
                    break;
                }
                case canvas::EventKind::NodeDeleted:
                {
                    doc.command_stack.push(
                        std::make_unique<DeleteNodeCommand>(NodeId(ev.node.v)),
                        doc.graph);
                    doc.dirty     = true;
                    dirty_rebuild = true;
                    break;
                }
                case canvas::EventKind::LinkCreated:
                {
                    PinRef const from = doc.adapter.pin_id_to_ref(ev.pin_from);
                    PinRef const to   = doc.adapter.pin_id_to_ref(ev.pin_to);
                    if (from.attr.empty() or to.attr.empty())
                    {
                        break;
                    }
                    std::string data_type;
                    Node const* fn = doc.graph.find_node(from.node);
                    if (fn != nullptr)
                    {
                        Attribute const* a = fn->find_attr(from.attr);
                        if (a != nullptr)
                        {
                            data_type = a->data_type;
                        }
                    }
                    doc.command_stack.push(
                        std::make_unique<CreateLinkCommand>(from, to, data_type),
                        doc.graph);
                    doc.dirty     = true;
                    dirty_rebuild = true;
                    break;
                }
                case canvas::EventKind::CopyRequested:
                {
                    copy_to_clipboard(doc, ev.selection);
                    break;
                }
                case canvas::EventKind::PasteRequested:
                {
                    doc.command_stack.open_group();
                    if (paste_from_clipboard(doc, ev.pos))
                    {
                        dirty_rebuild = true;
                    }
                    doc.command_stack.close_group();
                    break;
                }
                case canvas::EventKind::UndoRequested:
                {
                    if (doc.command_stack.can_undo())
                    {
                        doc.command_stack.undo(doc.graph);
                        doc.dirty     = true;
                        dirty_rebuild = true;
                    }
                    break;
                }
                case canvas::EventKind::RedoRequested:
                {
                    if (doc.command_stack.can_redo())
                    {
                        doc.command_stack.redo(doc.graph);
                        doc.dirty     = true;
                        dirty_rebuild = true;
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
            doc.command_stack.close_group();
        }
        if (dirty_rebuild)
        {
            doc.adapter.rebuild();
        }
    }
}
