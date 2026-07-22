#include <algorithm>
#include <cstdio>
#include <cstdlib>
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
#include <implot.h>
#include <imgui_stdlib.h>
#include <portable-file-dialogs.h>

#include "piper/app/main_window.h"

#include "piper/app/autosave.h"
#include "piper/app/bundled_fonts.h"
#include "piper/app/bundled_licenses.h"
#include "piper/app/minimap.h"
#include "piper/app/node_packs.h"
#include "piper/app/project_license.h"
#include "piper/app/settings.h"
#include "piper/app/theme_loader.h"
#include "piper/builtin_nodes.h"
#include "piper/canvas/event.h"
#include "piper/commands.h"
#include "piper/connect.h"
#include "piper/serialize_v2.h"

namespace piper::studio
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

    std::vector<std::string> scan_system_fonts()
    {
        std::vector<std::filesystem::path> roots;
        char const* home = std::getenv("HOME");
        if (home != nullptr)
        {
            roots.emplace_back(std::string(home) + "/.local/share/fonts");
            roots.emplace_back(std::string(home) + "/.fonts");
        }
        roots.emplace_back("/usr/share/fonts");
        roots.emplace_back("/usr/local/share/fonts");

        std::vector<std::string> out;
        for (auto const& root : roots)
        {
            std::error_code ec;
            if (not std::filesystem::is_directory(root, ec))
            {
                continue;
            }
            for (auto it = std::filesystem::recursive_directory_iterator(
                     root, std::filesystem::directory_options::skip_permission_denied, ec);
                 not ec and it != std::filesystem::recursive_directory_iterator{};
                 it.increment(ec))
            {
                auto const& p = it->path();
                auto const ext = p.extension().string();
                if (ext == ".ttf" or ext == ".otf"
                    or ext == ".TTF" or ext == ".OTF")
                {
                    out.push_back(p.string());
                }
            }
        }
        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }

    void MainWindow::align_selection(Document& doc, AlignMode mode)
    {
        std::vector<std::pair<NodeId, Point>> targets;
        targets.reserve(doc.editor.selection_ids().size());
        for (canvas::NodeId const cn : doc.editor.selection_ids())
        {
            Node const* n = doc.graph.find_node(NodeId(cn.v));
            if (n != nullptr)
            {
                targets.emplace_back(n->id, n->pos);
            }
        }
        if (targets.size() < 2)
        {
            return;
        }

        float min_x = targets.front().second.x;
        float max_x = min_x;
        float min_y = targets.front().second.y;
        float max_y = min_y;
        float sum_x = 0.0f;
        float sum_y = 0.0f;
        for (auto const& t : targets)
        {
            min_x = std::min(min_x, t.second.x);
            max_x = std::max(max_x, t.second.x);
            min_y = std::min(min_y, t.second.y);
            max_y = std::max(max_y, t.second.y);
            sum_x += t.second.x;
            sum_y += t.second.y;
        }
        float const mean_x = sum_x / float(targets.size());
        float const mean_y = sum_y / float(targets.size());

        doc.command_stack.open_group();
        for (auto const& t : targets)
        {
            Point np = t.second;
            switch (mode)
            {
                case AlignMode::Left:    { np.x = min_x;  break; }
                case AlignMode::Right:   { np.x = max_x;  break; }
                case AlignMode::Top:     { np.y = min_y;  break; }
                case AlignMode::Bottom:  { np.y = max_y;  break; }
                case AlignMode::CenterH: { np.y = mean_y; break; }
                case AlignMode::CenterV: { np.x = mean_x; break; }
            }
            if (np != t.second)
            {
                doc.command_stack.push(std::make_unique<MoveNodeCommand>(t.first, np), doc.graph);
            }
        }
        doc.command_stack.close_group();
        doc.dirty = true;
        doc.lint_dirty = true;
        doc.adapter.rebuild();
    }

    void MainWindow::distribute_selection(Document& doc, bool horizontal)
    {
        std::vector<std::pair<NodeId, Point>> targets;
        for (canvas::NodeId const cn : doc.editor.selection_ids())
        {
            Node const* n = doc.graph.find_node(NodeId(cn.v));
            if (n != nullptr)
            {
                targets.emplace_back(n->id, n->pos);
            }
        }
        if (targets.size() < 3)
        {
            return;
        }
        std::sort(targets.begin(), targets.end(),
                  [horizontal](auto const& a, auto const& b)
                  {
                      if (horizontal) { return a.second.x < b.second.x; }
                      return a.second.y < b.second.y;
                  });
        float start = targets.front().second.y;
        float end   = targets.back().second.y;
        if (horizontal)
        {
            start = targets.front().second.x;
            end   = targets.back().second.x;
        }
        float const step = (end - start) / float(targets.size() - 1);

        doc.command_stack.open_group();
        for (std::size_t i = 0; i < targets.size(); ++i)
        {
            Point np = targets[i].second;
            float const target = start + step * float(i);
            if (horizontal) { np.x = target; }
            else            { np.y = target; }
            if (np != targets[i].second)
            {
                doc.command_stack.push(std::make_unique<MoveNodeCommand>(targets[i].first, np), doc.graph);
            }
        }
        doc.command_stack.close_group();
        doc.dirty = true;
        doc.lint_dirty = true;
        doc.adapter.rebuild();
    }

    void MainWindow::poll_autosave()
    {
        constexpr auto interval = std::chrono::seconds(30);
        auto const now = std::chrono::steady_clock::now();
        if (now - autosave_last_check_ < std::chrono::seconds(5))
        {
            return;
        }
        autosave_last_check_ = now;
        for (auto& doc : documents_)
        {
            if (not doc->dirty)
            {
                continue;
            }
            if (doc->last_autosave_at.time_since_epoch().count() == 0
                or now - doc->last_autosave_at >= interval)
            {
                autosave_write(*doc);
            }
        }
    }

    void MainWindow::refresh_after_pack_load()
    {
        for (auto& doc : documents_)
        {
            // First pass: collect nodes whose saved type now resolves
            // and drop their UnknownNodeType diagnostic. Second pass:
            // run check_attribute_drift on each so AttributeMissing /
            // AttributeAdded / AttributeDrift diagnostics that would
            // have been emitted at load (had the type been known)
            // surface now without a reload.
            std::vector<NodeId> resolved_ids;
            auto const remove_resolved = [&](Diagnostic const& d)
            {
                if (d.kind != Diagnostic::Kind::UnknownNodeType)
                {
                    return false;
                }
                Node const* n = doc->graph.find_node(d.node_id);
                if (n == nullptr) { return false; }
                if (registry_.find(n->type) == nullptr) { return false; }
                resolved_ids.push_back(d.node_id);
                return true;
            };
            std::size_t const before = doc->diagnostics.size();
            doc->diagnostics.erase(
                std::remove_if(doc->diagnostics.begin(),
                                doc->diagnostics.end(),
                                remove_resolved),
                doc->diagnostics.end());
            for (NodeId id : resolved_ids)
            {
                Node const* n = doc->graph.find_node(id);
                if (n == nullptr) { continue; }
                NodeType const* spec = registry_.find(n->type);
                if (spec == nullptr) { continue; }
                piper::v2::check_attribute_drift(*n, *spec, doc->diagnostics);
            }
            if (doc->diagnostics.size() != before
                or not resolved_ids.empty())
            {
                doc->lint_dirty = true;
                doc->adapter.rebuild();
            }
        }
    }

    void MainWindow::touch_recent_file(std::string const& path)
    {
        if (path.empty())
        {
            return;
        }
        std::error_code ec;
        std::string const abs = std::filesystem::absolute(path, ec).string();
        std::string key = abs;
        if (ec)
        {
            key = path;
        }
        recent_files_.erase(
            std::remove(recent_files_.begin(), recent_files_.end(), key),
            recent_files_.end());
        recent_files_.insert(recent_files_.begin(), key);
        constexpr std::size_t cap = 10;
        if (recent_files_.size() > cap)
        {
            recent_files_.resize(cap);
        }
        Settings s;
        s.recent_files = recent_files_;
        save_settings(s);
    }

    void MainWindow::push_toast(ToastLevel level, std::string message)
    {
        Toast t;
        t.level   = level;
        t.message = std::move(message);
        t.spawned = std::chrono::steady_clock::now();
        toasts_.push_back(std::move(t));
    }

    MainWindow::MainWindow(float dpi_scale)
    {
        if (dpi_scale > 0.0f)
        {
            dpi_scale_ = dpi_scale;
        }
        inspector_width_     *= dpi_scale_;
        inspector_min_width_ *= dpi_scale_;
        register_builtin_nodes(registry_);
        piper::engine::register_builtin_steps(step_registry_);
        // External node packs from $XDG_CONFIG_HOME/piper/nodes (or
        // $HOME/.config/piper/nodes). Builtins win on duplicate names.
        {
            NodePackLoadResult const r = auto_load_node_packs(registry_);
            if (r.added > 0)
            {
                push_toast(ToastLevel::Info,
                           "Loaded " + std::to_string(r.added)
                               + " external node type(s)");
            }
            for (auto const& w : r.warnings)
            {
                push_toast(ToastLevel::Warn, w);
            }
            for (auto const& e : r.errors)
            {
                push_toast(ToastLevel::Error, e);
            }
        }
        try_load_theme();

        Settings const s = load_settings();
        if (s.recent_files.has_value())
        {
            recent_files_ = *s.recent_files;
        }
        if (s.font_path.has_value())
        {
            theme_.font_path = *s.font_path;
        }
        if (s.font_size.has_value())
        {
            theme_.font_size = *s.font_size;
        }
        if (not theme_.font_path.empty()
            and not theme_.font_path.starts_with("bundled:"))
        {
            std::error_code ec;
            if (not std::filesystem::exists(theme_.font_path, ec))
            {
                push_toast(ToastLevel::Warn,
                           "Saved font '" + theme_.font_path
                               + "' not found, using default");
                theme_.font_path.clear();
                Settings cleared;
                cleared.font_path = theme_.font_path;
                cleared.font_size = theme_.font_size;
                save_settings(cleared);
            }
        }

        apply_current_theme();

        autosave_pending_ = scan_autosave_dir();
        if (not autosave_pending_.empty())
        {
            autosave_recovery_open_ = true;
        }
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
        doc->session_id = next_session_id_++;
        wire_document_callbacks(*doc);
        doc->editor.set_style(canvas_style_);
        doc->editor.set_layout(canvas_layout_);
        doc->adapter.set_color_pins_by_stage(color_pins_by_stage_);
        doc->adapter.rebuild();
        Document& ref = *doc;
        documents_.push_back(std::move(doc));
        active_doc_idx_ = int(documents_.size()) - 1;
        ++next_untitled_id_;
        return ref;
    }

    ImU32 to_im_alpha(rgba c, float alpha_mul)
    {
        uint32_t r = c.r();
        uint32_t g = c.g();
        uint32_t b = c.b();
        uint32_t a = uint32_t(float(c.a()) * alpha_mul);
        if (a > 255u) { a = 255u; }
        return IM_COL32(r, g, b, a);
    }

    // The "Add node" menu tree. A node type's `category` is a '/'-
    // delimited path, so users grouping their own node packs (e.g.
    // "hal/motor") get nested submenus. Empty category -> the type
    // sits at the root level. std::map keeps submenus name-sorted.
    struct AddNodeMenuTree
    {
        std::map<std::string, AddNodeMenuTree> children;
        std::vector<piper::NodeType const*>    types;
    };

    void sort_add_node_tree(AddNodeMenuTree& tree)
    {
        std::sort(tree.types.begin(), tree.types.end(),
                  [](piper::NodeType const* a, piper::NodeType const* b)
                  {
                      return a->type < b->type;
                  });
        for (auto& kv : tree.children)
        {
            sort_add_node_tree(kv.second);
        }
    }

    AddNodeMenuTree build_add_node_tree(
        std::vector<piper::NodeType const*> const& types)
    {
        AddNodeMenuTree root;
        for (auto const* nt : types)
        {
            AddNodeMenuTree* cur = &root;
            std::string_view const cat = nt->category;
            std::size_t start = 0;
            while (start < cat.size())
            {
                std::size_t const slash = cat.find('/', start);
                std::size_t end = cat.size();
                if (slash != std::string_view::npos)
                {
                    end = slash;
                }
                std::string_view const seg = cat.substr(start, end - start);
                if (not seg.empty())
                {
                    cur = &cur->children[std::string(seg)];
                }
                if (slash == std::string_view::npos)
                {
                    break;
                }
                start = slash + 1;
            }
            cur->types.push_back(nt);
        }
        sort_add_node_tree(root);
        return root;
    }

    // Returns the type the user clicked this frame, or nullptr. The
    // caller owns node creation so this stays free of Document state.
    piper::NodeType const* draw_add_node_tree(AddNodeMenuTree const& tree)
    {
        piper::NodeType const* chosen = nullptr;
        for (auto const* nt : tree.types)
        {
            if (ImGui::MenuItem(nt->type.c_str()))
            {
                chosen = nt;
            }
        }
        if (not tree.types.empty() and not tree.children.empty())
        {
            ImGui::Separator();
        }
        for (auto const& kv : tree.children)
        {
            if (ImGui::BeginMenu(kv.first.c_str()))
            {
                piper::NodeType const* const c = draw_add_node_tree(kv.second);
                if (c != nullptr)
                {
                    chosen = c;
                }
                ImGui::EndMenu();
            }
        }
        return chosen;
    }

    void MainWindow::wire_document_callbacks(Document& doc)
    {
        Document* dp = &doc;
        doc.editor.set_body_renderer(
            [dp](canvas::NodeId nid, ImDrawList* draw_list,
                 ImVec2 const& rect_min, ImVec2 const& rect_max, float zoom)
            {
                (void)rect_max;
                auto const it = dp->probe_latest.find(piper::NodeId{ nid.v });
                if (it == dp->probe_latest.end())
                {
                    return;
                }
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.4g", it->second);
                ImFont* const font      = ImGui::GetFont();
                float   const font_size = ImGui::GetFontSize() * zoom;
                ImVec2 const text_pos{
                    rect_min.x + 8.0f * zoom,
                    rect_min.y + 2.0f * zoom,
                };
                draw_list->AddText(font, font_size, text_pos,
                                   IM_COL32(0xE0, 0xFF, 0xE0, 0xFF),
                                   buf, buf + std::strlen(buf));
            });
        doc.editor.set_background_renderer(
            [dp](ImDrawList* draw_list, ImVec2 const& origin,
                 ImVec2 const& size, float zoom, ImVec2 const& pan)
            {
                (void)size;
                ImFont* const font      = ImGui::GetFont();
                float   const font_size = ImGui::GetFontSize() * zoom;
                for (auto const& a : dp->graph.annotations())
                {
                    ImVec2 const tl{
                        origin.x + (a.pos.x - pan.x) * zoom,
                        origin.y + (a.pos.y - pan.y) * zoom,
                    };
                    ImVec2 const br{
                        tl.x + a.size.x * zoom,
                        tl.y + a.size.y * zoom,
                    };
                    ImU32 const fill   = to_im_alpha(a.color, 1.0f);
                    ImU32 const border = to_im_alpha(a.color.with_alpha(0xFF), 1.0f);
                    draw_list->AddRectFilled(tl, br, fill, 4.0f);
                    draw_list->AddRect(tl, br, border, 4.0f, 0, 1.5f * zoom);
                    if (not a.text.empty())
                    {
                        ImVec2 const text_pos{ tl.x + 6.0f * zoom, tl.y + 4.0f * zoom };
                        draw_list->AddText(font, font_size, text_pos,
                                           IM_COL32(0xFF, 0xFF, 0xFF, 0xFF),
                                           a.text.data(),
                                           a.text.data() + a.text.size());
                    }
                }
            });
        doc.editor.set_context_menu([this, dp](canvas::NodeId hovered, ImVec2 const& canvas_pos)
        {
            if (canvas::PinId const cpin = dp->editor.context_menu_pin();
                cpin != canvas::invalid_pin_id)
            {
                PinRef const ref = dp->adapter.pin_id_to_ref(cpin);
                if (not ref.attr.empty())
                {
                    if (ImGui::MenuItem("Flip pin side"))
                    {
                        bool cur = false;
                        if (Node const* n = dp->graph.find_node(ref.node); n != nullptr)
                        {
                            if (Attribute const* a = n->find_attr(ref.attr); a != nullptr)
                            {
                                cur = a->flip_side;
                            }
                        }
                        dp->command_stack.push(
                            std::make_unique<SetPinFlipSideCommand>(ref.node, ref.attr, not cur),
                            dp->graph);
                        dp->adapter.rebuild();
                        dp->dirty = true;
                    }
                    ImGui::Separator();
                }
            }

            if (hovered == canvas::invalid_node_id)
            {
                AnnotationId hovered_anno = invalid_annotation_id;
                for (auto const& a : dp->graph.annotations())
                {
                    if (canvas_pos.x >= a.pos.x and canvas_pos.x < a.pos.x + a.size.x
                        and canvas_pos.y >= a.pos.y and canvas_pos.y < a.pos.y + a.size.y)
                    {
                        hovered_anno = a.id;
                        break;
                    }
                }
                if (hovered_anno != invalid_annotation_id)
                {
                    Annotation const* a = dp->graph.find_annotation(hovered_anno);
                    if (a != nullptr)
                    {
                        ImGui::Text("Annotation");
                        ImGui::Separator();
                        if (ImGui::MenuItem("Edit..."))
                        {
                            dp->popup.editing_annotation = hovered_anno;
                        }
                        if (ImGui::MenuItem("Delete annotation"))
                        {
                            dp->command_stack.push(std::make_unique<DeleteAnnotationCommand>(hovered_anno), dp->graph);
                            dp->dirty      = true;
                            dp->lint_dirty = true;
                        }
                        return;
                    }
                }

                if (ImGui::MenuItem("Add annotation"))
                {
                    Annotation a;
                    a.pos  = Point{ canvas_pos.x, canvas_pos.y };
                    a.text = "Note";
                    dp->command_stack.push(std::make_unique<AddAnnotationCommand>(a), dp->graph);
                    dp->dirty      = true;
                    dp->lint_dirty = true;
                }

                if (ImGui::BeginMenu("Add label"))
                {
                    if (ImGui::MenuItem("Source"))
                    {
                        dp->command_stack.push(std::make_unique<AddLabelCommand>(
                            LabelKind::In, std::string{},
                            Point{ canvas_pos.x, canvas_pos.y }), dp->graph);
                        dp->dirty      = true;
                        dp->lint_dirty = true;
                        dp->adapter.rebuild();
                    }
                    if (ImGui::MenuItem("Sink"))
                    {
                        dp->command_stack.push(std::make_unique<AddLabelCommand>(
                            LabelKind::Out, std::string{},
                            Point{ canvas_pos.x, canvas_pos.y }), dp->graph);
                        dp->dirty      = true;
                        dp->lint_dirty = true;
                        dp->adapter.rebuild();
                    }
                    ImGui::EndMenu();
                }

                if (ImGui::BeginMenu("Add node"))
                {
                    AddNodeMenuTree const tree = build_add_node_tree(registry_.all());
                    if (piper::NodeType const* nt = draw_add_node_tree(tree))
                    {
                        add_node_at(*dp, *nt, canvas_pos);
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

            // Label: separate context-menu path. Labels share the
            // NodeId space with nodes; check labels before falling
            // through to the node menu.
            if (Label const* lbl = dp->graph.find_label(NodeId(hovered.v));
                lbl != nullptr)
            {
                char const* label_title = "(unnamed)";
                if (not lbl->name.empty()) { label_title = lbl->name.c_str(); }
                ImGui::Text("Label: %s", label_title);
                ImGui::Separator();
                if (ImGui::MenuItem("Delete label"))
                {
                    dp->command_stack.push(std::make_unique<DeleteLabelCommand>(NodeId(hovered.v)), dp->graph);
                    dp->dirty      = true;
                    dp->lint_dirty = true;
                    dp->adapter.rebuild();
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
                    bool const unset_sel = node->stage.empty();
                    if (ImGui::MenuItem("(unset)", nullptr, unset_sel)
                        and not unset_sel)
                    {
                        dp->command_stack.push(std::make_unique<SetNodeStageCommand>(
                            node->id, std::string{}), dp->graph);
                        dp->dirty      = true;
                        dp->lint_dirty = true;
                        dp->adapter.rebuild();
                    }
                    for (auto const& s : dp->graph.stages())
                    {
                        bool const sel = (s.name == node->stage);
                        if (ImGui::MenuItem(s.name.c_str(), nullptr, sel)
                            and not sel)
                        {
                            dp->command_stack.push(std::make_unique<SetNodeStageCommand>(
                                node->id, s.name), dp->graph);
                            dp->dirty      = true;
                            dp->lint_dirty = true;
                            dp->adapter.rebuild();
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
                        dp->command_stack.push(std::make_unique<SetNodeModeLabelCommand>(
                            dp->active_mode_profile, node->id, label), dp->graph);
                        dp->dirty = true;
                    dp->lint_dirty = true;
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

                    auto const advertised = piper::mode_labels_advertised_by(*node, registry_);
                    if (not advertised.empty())
                    {
                        ImGui::Separator();
                        ImGui::TextDisabled("from this node:");
                        for (auto const& lbl : advertised)
                        {
                            bool const sel = (current_label == lbl);
                            if (ImGui::MenuItem(lbl.c_str(), nullptr, sel) and not sel)
                            {
                                apply_label(lbl);
                            }
                        }
                    }

                    bool theme_started = false;
                    for (auto const& kv : theme_.mode_colors)
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
                    }
                    ImGui::EndMenu();
                }
            }
        });
        doc.editor.set_extra_hit_test([dp](ImVec2 const& canvas_pos) -> bool
        {
            for (auto const& a : dp->graph.annotations())
            {
                if (canvas_pos.x >= a.pos.x and canvas_pos.x < a.pos.x + a.size.x
                    and canvas_pos.y >= a.pos.y and canvas_pos.y < a.pos.y + a.size.y)
                {
                    return true;
                }
            }
            return false;
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
            push_toast(ToastLevel::Error,
                       std::string{"Load failed: "} + e.what());
            return false;
        }

        if (bundle.pipelines.empty())
        {
            std::fprintf(stderr, "load: %s contained no pipelines\n", path.c_str());
            push_toast(ToastLevel::Warn,
                       std::string{"'"} + path + "' contained no pipelines");
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
            target->editor.request_fit();

            for (auto const& d : target->diagnostics)
            {
                if (d.kind == Diagnostic::Kind::LabelClusterRepaired)
                {
                    push_toast(ToastLevel::Info, d.message);
                    continue;
                }
                std::fprintf(stderr, "diagnostic: %s\n", d.message.c_str());
            }
        }
        for (auto const& d : bundle.diagnostics)
        {
            std::fprintf(stderr, "bundle diagnostic: %s\n", d.message.c_str());
        }
        touch_recent_file(path);
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
        canvas_style_  = canvas::Style{};
        canvas_layout_ = canvas::LayoutMetrics{};
        apply_theme(theme_, canvas_style_, ImGui::GetStyle());

        canvas_style_.grid_spacing         *= dpi_scale_;
        canvas_style_.node_rounding        *= dpi_scale_;
        canvas_style_.node_padding.x       *= dpi_scale_;
        canvas_style_.node_padding.y       *= dpi_scale_;
        canvas_style_.pin_radius           *= dpi_scale_;
        canvas_style_.link_thickness       *= dpi_scale_;
        canvas_style_.link_bezier_strength *= dpi_scale_;

        canvas_layout_.header_height   *= dpi_scale_;
        canvas_layout_.pin_row_height  *= dpi_scale_;
        canvas_layout_.min_width       *= dpi_scale_;
        canvas_layout_.min_body_height *= dpi_scale_;
        canvas_layout_.label_padding   *= dpi_scale_;

        for (auto& doc : documents_)
        {
            doc->editor.set_style(canvas_style_);
            doc->editor.set_layout(canvas_layout_);
        }
    }

    bool MainWindow::consume_font_reload(std::string& path, float& size)
    {
        if (not wants_font_reload_)
        {
            return false;
        }
        wants_font_reload_ = false;
        path = theme_.font_path;
        size = theme_.font_size;
        return true;
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
            bool const font_changed =
                  result.theme.font_path != theme_.font_path
               or result.theme.font_size != theme_.font_size;
            theme_ = std::move(result.theme);
            apply_current_theme();
            if (font_changed)
            {
                wants_font_reload_ = true;
            }
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
            push_toast(ToastLevel::Error,
                       std::string{"Theme reload failed: "} + e.what());
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
                *nt, e.node.name, e.node.stage, new_pos);
            AddNodeCommand* raw = cmd.get();
            doc.command_stack.push(std::move(cmd), doc.graph);
            NodeId const new_id = raw->node_id();
            id_map[e.node.id] = new_id;
            for (auto const& a : e.node.attrs)
            {
                if (a.role == AttributeSpec::Role::Member or a.stages.empty())
                {
                    continue;
                }
                doc.command_stack.push(std::make_unique<SetAttributeStagesCommand>(new_id, a.name, a.stages), doc.graph);
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
            doc.command_stack.push(std::make_unique<CreateLinkCommand>(nf, nt, l.data_type), doc.graph);
        }
        doc.dirty = true;
        doc.lint_dirty = true;
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

    void MainWindow::draw_live_panel(Document& doc)
    {
        // Status banner so the user knows whether the panel is reading
        // a live engine or showing the captured-and-frozen state from
        // the previous run.
        if (doc.engine_running)
        {
            ImGui::TextDisabled("running -- ticking each frame");
        }
        else
        {
            bool const has_history = not doc.probe_history.empty();
            if (has_history)
            {
                ImGui::TextDisabled("stopped -- last captured curves shown below");
            }
            else
            {
                ImGui::TextDisabled("Click Run on the toolbar to start the engine.");
                ImGui::TextDisabled("This panel drives external_input<*> nodes and");
                ImGui::TextDisabled("shows scrolling plots of every external_output<float>.");
                return;
            }
        }
        ImGui::Separator();

        // Active mode profile. Pushes through to Engine::set_mode so
        // preset3 (and other label-aware steps) switch slots
        // immediately, and through the canvas adapter so the body-
        // color overlay updates in lockstep with the inspector /
        // modes panel.
        if (not doc.graph.mode_profiles().empty())
        {
            char const* preview = "(none)";
            if (not doc.active_mode_profile.empty())
            {
                preview = doc.active_mode_profile.c_str();
            }
            ImGui::TextUnformatted("Profile");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::BeginCombo("##live_profile", preview))
            {
                auto const apply = [&](std::string const& new_name)
                {
                    doc.active_mode_profile = new_name;
                    if (doc.engine != nullptr)
                    {
                        doc.engine->set_mode(new_name);
                    }
                    doc.adapter.set_active_mode_profile(new_name);
                    doc.adapter.rebuild();
                    doc.lint_dirty = true;
                };
                bool const none_sel = doc.active_mode_profile.empty();
                if (ImGui::Selectable("(none)", none_sel) and not none_sel)
                {
                    apply(std::string{});
                }
                for (auto const& mp : doc.graph.mode_profiles())
                {
                    bool const sel = (doc.active_mode_profile == mp.name);
                    if (ImGui::Selectable(mp.name.c_str(), sel) and not sel)
                    {
                        apply(mp.name);
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::Spacing();
        }

        auto const attr_value = [](Node const& n, std::string_view name) -> char const*
        {
            for (auto const& a : n.attrs)
            {
                if (a.role == AttributeSpec::Role::Member and a.name == name)
                {
                    return a.value.c_str();
                }
            }
            return "";
        };

        // Inputs section. Walk every external_input<*> in the graph,
        // surface a slider that writes to doc.live_input_*. The next
        // tick_engine_live() pushes the value to the engine before
        // play().
        ImGui::TextUnformatted("Inputs");
        ImGui::Separator();
        bool any_input = false;
        for (auto const& n : doc.graph.nodes())
        {
            if (n.type == "external_input<float>")
            {
                std::string const ext_name = attr_value(n, "name");
                if (ext_name.empty())
                {
                    continue;
                }
                any_input = true;
                float& val = doc.live_input_float[ext_name];
                float lo = -1.0f;
                float hi =  1.0f;
                try { lo = std::stof(attr_value(n, "min")); } catch (...) {}
                try { hi = std::stof(attr_value(n, "max")); } catch (...) {}
                if (hi <= lo) { hi = lo + 1.0f; }
                ImGui::PushID(int(n.id));
                ImGui::SetNextItemWidth(-FLT_MIN);
                // SliderFloat: visible position within [lo, hi]; Ctrl+click
                // for direct numeric entry (inputs not strictly clamped).
                ImGui::SliderFloat(ext_name.c_str(), &val, lo, hi, "%.4f",
                                   ImGuiSliderFlags_AlwaysClamp);
                ImGui::PopID();
            }
            else if (n.type == "external_input<int32_t>")
            {
                std::string const ext_name = attr_value(n, "name");
                if (ext_name.empty())
                {
                    continue;
                }
                any_input = true;
                int32_t& val = doc.live_input_int[ext_name];
                int lo = -100;
                int hi =  100;
                try { lo = std::stoi(attr_value(n, "min")); } catch (...) {}
                try { hi = std::stoi(attr_value(n, "max")); } catch (...) {}
                if (hi <= lo) { hi = lo + 1; }
                ImGui::PushID(int(n.id));
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::SliderInt(ext_name.c_str(), &val, lo, hi, "%d",
                                 ImGuiSliderFlags_AlwaysClamp);
                ImGui::PopID();
            }
        }
        if (not any_input)
        {
            ImGui::TextDisabled("(no external_input nodes in the graph)");
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Probes");
        ImGui::SameLine();
        ImGui::Checkbox("merge", &doc.live_merge_probe_plots);
        ImGui::Separator();

        // High-contrast palette so adjacent probes differ at a glance
        // without relying on ImPlot's default first-series blue, which
        // is hard to read against the dark plot background.
        static constexpr ImU32 probe_palette[] = {
            IM_COL32(0x9C, 0xE6, 0x55, 0xFF),   // lime
            IM_COL32(0xFF, 0xCC, 0x40, 0xFF),   // amber
            IM_COL32(0x66, 0xCC, 0xFF, 0xFF),   // cyan
            IM_COL32(0xFF, 0x80, 0x80, 0xFF),   // salmon
            IM_COL32(0xC0, 0x9C, 0xFF, 0xFF),   // lavender
            IM_COL32(0xFF, 0xB0, 0x60, 0xFF),   // peach
        };
        constexpr std::size_t palette_size =
            sizeof(probe_palette) / sizeof(probe_palette[0]);

        // Pre-collect probes so the header colors and the plot lines
        // share the same palette index in one pass. References into
        // doc.probe_history are stable for this draw.
        struct ProbeView
        {
            std::string const* name;
            std::vector<float> const* hist;
            ImU32              color;
        };
        std::vector<ProbeView> probes;
        std::vector<float> const empty;
        for (auto const& n : doc.graph.nodes())
        {
            if (n.type != "external_output<float>")
            {
                continue;
            }
            auto const it = doc.probe_history.find(n.id);
            ProbeView pv;
            pv.name  = &n.name;
            pv.hist  = &empty;
            if (it != doc.probe_history.end())
            {
                pv.hist = &it->second;
            }
            pv.color = probe_palette[probes.size() % palette_size];
            probes.push_back(pv);
        }
        bool const any_probe = not probes.empty();

        for (auto const& p : probes)
        {
            float last = 0.0f;
            if (not p.hist->empty())
            {
                last = p.hist->back();
            }
            ImGui::PushStyleColor(ImGuiCol_Text, p.color);
            ImGui::Text("%s = %.4g", p.name->c_str(), last);
            ImGui::PopStyleColor();
        }

        if (any_probe)
        {
            constexpr ImPlotFlags     plot_flags = ImPlotFlags_NoTitle
                                                 | ImPlotFlags_NoLegend
                                                 | ImPlotFlags_NoMouseText;
            constexpr ImPlotAxisFlags x_flags    = ImPlotAxisFlags_NoTickLabels;
            constexpr ImPlotAxisFlags y_flags    = ImPlotAxisFlags_AutoFit;

            // Fill all remaining vertical space; resize the right
            // sidebar (drag its splitter) to grow the plots.
            float const remaining_h = ImGui::GetContentRegionAvail().y;

            if (doc.live_merge_probe_plots)
            {
                // Phase relationships (setpoint vs measured vs out,
                // etc.) read immediately when the curves share an axis.
                std::size_t longest = 0;
                for (auto const& p : probes)
                {
                    if (p.hist->size() > longest) { longest = p.hist->size(); }
                }
                float const h = std::max(80.0f, remaining_h);
                if (ImPlot::BeginPlot("##probes_merged", ImVec2(-1.0f, h), plot_flags))
                {
                    ImPlot::SetupAxes(nullptr, nullptr, x_flags, y_flags);
                    if (longest >= 2)
                    {
                        ImPlot::SetupAxisLimits(ImAxis_X1, 0.0,
                                                static_cast<double>(longest - 1),
                                                ImPlotCond_Always);
                        for (auto const& p : probes)
                        {
                            if (p.hist->size() < 2) { continue; }
                            ImPlot::SetNextLineStyle(
                                ImGui::ColorConvertU32ToFloat4(p.color), 1.6f);
                            ImPlot::PlotLine(p.name->c_str(),
                                             p.hist->data(),
                                             static_cast<int>(p.hist->size()));
                        }
                    }
                    ImPlot::EndPlot();
                }
            }
            else
            {
                // One plot per probe; split remaining height equally.
                // Floor at 80 px so plots stay legible on small windows;
                // the panel's child window scrolls vertically beyond.
                float const spacing = ImGui::GetStyle().ItemSpacing.y;
                float const slot_h  = remaining_h / float(probes.size())
                                    - spacing;
                float const per_h   = std::max(80.0f, slot_h);
                for (std::size_t i = 0; i < probes.size(); ++i)
                {
                    auto const& p = probes[i];
                    std::string const id = std::string("##plot_")
                                         + std::to_string(i);
                    if (ImPlot::BeginPlot(id.c_str(), ImVec2(-1.0f, per_h), plot_flags))
                    {
                        ImPlot::SetupAxes(nullptr, nullptr, x_flags, y_flags);
                        if (p.hist->size() >= 2)
                        {
                            ImPlot::SetupAxisLimits(
                                ImAxis_X1, 0.0,
                                static_cast<double>(p.hist->size() - 1),
                                ImPlotCond_Always);
                            ImPlot::SetNextLineStyle(
                                ImGui::ColorConvertU32ToFloat4(p.color), 1.6f);
                            ImPlot::PlotLine(p.name->c_str(),
                                             p.hist->data(),
                                             static_cast<int>(p.hist->size()));
                        }
                        ImPlot::EndPlot();
                    }
                    ImGui::Spacing();
                }
            }
        }
        if (not any_probe)
        {
            ImGui::TextDisabled("(no external_output<float> nodes in the graph)");
        }
    }

    void MainWindow::toggle_engine_run(Document& doc)
    {
        if (doc.engine_running)
        {
            // Stop: keep probe_latest + probe_history so the Live panel
            // can keep showing the curves the user just captured. The
            // engine itself is torn down so build() can run cleanly on
            // the next Start.
            doc.engine_running         = false;
            doc.engine.reset();
            doc.engine_built_revision  = 0;
            return;
        }
        doc.engine = std::make_unique<piper::engine::Engine>();
        doc.engine->set_name(doc.pipeline_name);
        auto const r = doc.engine->build(doc.graph, step_registry_);
        if (not r.ok)
        {
            std::string msg = "engine build failed:";
            for (auto const& d : r.diagnostics)
            {
                msg += "\n  - " + d.message;
            }
            push_toast(ToastLevel::Error, msg);
            doc.engine.reset();
            return;
        }
        // Run the mode selected in the UI; build() defaults to base mode.
        doc.engine->set_mode(doc.active_mode_profile);
        doc.engine_built_revision = doc.command_stack.revision();
        doc.engine_running        = true;
        // Fresh capture buffer on start: avoids a visual discontinuity
        // between the previous run's last samples and this run's first
        // ones (engine state has been rebuilt from scratch).
        doc.probe_latest.clear();
        doc.probe_history.clear();
        // Surface the Live tab + give it enough width for the probe
        // plots to be readable. Only widens; never shrinks past the
        // user's last splitter position.
        focus_live_tab_pending_ = true;
        constexpr float live_min_width = 380.0f;
        if (inspector_width_ < live_min_width)
        {
            inspector_width_ = live_min_width;
        }
    }

    void MainWindow::tick_engine_live(Document& doc)
    {
        if (not doc.engine_running or doc.engine == nullptr)
        {
            return;
        }
        // Hot-rebuild if the graph mutated since the last build. Step
        // state (integrals, filter histories) resets -- acceptable for
        // interactive tweaking; the user expects "tweak, see".
        if (doc.command_stack.revision() != doc.engine_built_revision)
        {
            doc.engine = std::make_unique<piper::engine::Engine>();
            doc.engine->set_name(doc.pipeline_name);
            auto const r = doc.engine->build(doc.graph, step_registry_);
            if (not r.ok)
            {
                std::string msg = "live engine rebuild failed:";
                for (auto const& d : r.diagnostics)
                {
                    msg += "\n  - " + d.message;
                }
                push_toast(ToastLevel::Error, msg);
                doc.engine.reset();
                doc.engine_running = false;
                return;
            }
            doc.engine->set_mode(doc.active_mode_profile);
            doc.engine_built_revision = doc.command_stack.revision();
            doc.probe_latest.clear();
            doc.probe_history.clear();
        }

        // Drive external_input<*> from the Live panel's slider state.
        for (auto const& [name, val] : doc.live_input_float)
        {
            if (auto* in = doc.engine->input<float>(name))
            {
                in->set(val);
            }
        }
        for (auto const& [name, val] : doc.live_input_int)
        {
            if (auto* in = doc.engine->input<int32_t>(name))
            {
                in->set(val);
            }
        }

        doc.engine->play();

        // Capture every external_output<float>'s latest value for the
        // canvas body renderer + scrolling plot. Bounded history --
        // ~10 s at 60 fps; cheap to keep around.
        constexpr std::size_t live_history_max = 600;
        for (auto const& n : doc.graph.nodes())
        {
            if (n.type != "external_output<float>")
            {
                continue;
            }
            auto const* step = doc.engine->step_for(n.id);
            if (step == nullptr)
            {
                continue;
            }
            try
            {
                float const v = step->input<float>("in");
                doc.probe_latest[n.id] = v;
                auto& h = doc.probe_history[n.id];
                h.push_back(v);
                if (h.size() > live_history_max)
                {
                    h.erase(h.begin(),
                            h.begin() + (h.size() - live_history_max));
                }
            }
            catch (std::exception const&)
            {
                // Pin not wired yet; nothing to capture.
            }
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

    std::vector<std::pair<NodeId, std::string>>
    MainWindow::unwired_required_inputs(Document const& doc) const
    {
        std::set<std::pair<NodeId, std::string>> connected;
        for (auto const& l : doc.graph.links())
        {
            connected.insert({ l.to.node,   l.to.attr });
            connected.insert({ l.from.node, l.from.attr });
        }

        std::vector<std::pair<NodeId, std::string>> missing;
        for (auto const& n : doc.graph.nodes())
        {
            // Optional inputs set is_optional in the spec and the step
            // falls back when unwired (e.g. dt_in on sin_wave/low_pass/pid).
            NodeType const* spec = registry_.find(n.type);
            for (auto const& a : n.attrs)
            {
                if (a.role != AttributeSpec::Role::Input)
                {
                    continue;
                }
                bool optional = false;
                if (spec != nullptr)
                {
                    for (auto const& s : spec->attributes)
                    {
                        if (s.name == a.name and s.is_optional)
                        {
                            optional = true;
                            break;
                        }
                    }
                }
                if (optional)
                {
                    continue;
                }
                if (connected.count({ n.id, a.name }) == 0)
                {
                    missing.push_back({ n.id, a.name });
                }
            }
        }
        return missing;
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
            if (stages_defined and n.stage.empty())
            {
                Diagnostic d;
                d.kind    = Diagnostic::Kind::SchemaError;
                d.message = "node '" + n.name + "' has no stage";
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
                d.kind    = Diagnostic::Kind::SchemaError;
                d.message = "node '" + n.name + "' is disconnected";
                d.node_id = n.id;
                doc.lint_diagnostics.push_back(d);
            }

            // Only flag missing per-node labels for nodes whose type
            // advertises mode labels (preset3 etc.). Ordinary nodes
            // don't dispatch on labels, so "no entry" means the engine
            // just runs them -- not worth a warning.
            if (active_profile != nullptr
                and active_profile->per_node.count(n.id) == 0
                and not mode_labels_advertised_by(n, registry_).empty())
            {
                Diagnostic d;
                d.kind    = Diagnostic::Kind::SchemaError;
                d.message = "node '" + n.name + "' has no label in profile '"
                          + doc.active_mode_profile + "' -- it advertises "
                          "computational slots that won't dispatch";
                d.node_id = n.id;
                doc.lint_diagnostics.push_back(d);
            }
        }

        for (auto const& [nid, attr] : unwired_required_inputs(doc))
        {
            std::string nname;
            if (Node const* n = doc.graph.find_node(nid); n != nullptr)
            {
                nname = n->name;
            }
            Diagnostic d;
            d.kind      = Diagnostic::Kind::SchemaError;
            d.message   = "input '" + nname + "." + attr + "' has no source";
            d.node_id   = nid;
            d.attr_name = attr;
            doc.lint_diagnostics.push_back(d);
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
        doc.command_stack.push(std::make_unique<AddNodeCommand>(type, name, doc.current_stage, pos), doc.graph);
        doc.dirty = true;
        doc.lint_dirty = true;
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
        std::filesystem::path const target{path};
        std::filesystem::path const tmp_path = target.string() + ".tmp";
        std::filesystem::path const bak_path = target.string() + ".bak";

        {
            std::ofstream f(tmp_path);
            if (not f.is_open())
            {
                std::fprintf(stderr, "could not open %s for writing\n",
                             tmp_path.string().c_str());
                return false;
            }
            f << json;
            if (not f)
            {
                std::fprintf(stderr, "write to %s failed\n",
                             tmp_path.string().c_str());
                return false;
            }
        }

        std::error_code ec;
        if (std::filesystem::exists(target, ec))
        {
            // Best-effort: replace previous backup with the soon-to-be-
            // overwritten file. Failure here is non-fatal.
            std::filesystem::remove(bak_path, ec);
            std::filesystem::rename(target, bak_path, ec);
            if (ec)
            {
                std::fprintf(stderr, "backup of %s failed: %s\n",
                             target.string().c_str(), ec.message().c_str());
                ec.clear();
            }
        }
        std::filesystem::rename(tmp_path, target, ec);
        if (ec)
        {
            std::string const msg = "rename " + tmp_path.string() + " -> "
                                  + target.string() + " failed: " + ec.message();
            std::fprintf(stderr, "%s\n", msg.c_str());
            push_toast(ToastLevel::Error, "Save failed: " + ec.message());
            return false;
        }

        push_toast(ToastLevel::Info, "Saved " + target.filename().string());
        doc.loaded_path = path;
        doc.dirty       = false;
        autosave_remove(doc);
        touch_recent_file(path);
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

    void MainWindow::request_quit()
    {
        // Just mark; the next draw() decides whether to pop a dialog
        // or quit straight away. Lets us be called from outside an
        // ImGui frame (e.g. main.cc when GLFW reports the window was
        // closed via the title-bar X).
        quit_requested_ = true;
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
        autosave_remove(*documents_[pending_close_idx_]);
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
        poll_autosave();
        tick_stage_play(doc);
        tick_engine_live(doc);
        if (doc.lint_dirty)
        {
            recompute_lints(doc);
            doc.lint_dirty = false;
        }

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

        // Resolve any pending quit attempt now that we're inside the
        // root window scope: ImGui::OpenPopup needs a current window
        // to bind the popup to. Covers all paths -- menu, shortcut,
        // GLFW close button funnelled through request_quit().
        if (quit_requested_)
        {
            quit_requested_ = false;
            bool any_dirty = false;
            for (auto const& d : documents_)
            {
                if (d->dirty) { any_dirty = true; break; }
            }
            if (any_dirty)
            {
                quit_confirming_ = true;
                ImGui::OpenPopup("##confirm_quit");
            }
            else
            {
                running_ = false;
            }
        }

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
                if (ImGui::BeginMenu("Open Recent", not recent_files_.empty()))
                {
                    std::string to_open;
                    for (auto const& path : recent_files_)
                    {
                        std::string const label =
                            std::filesystem::path(path).filename().string();
                        if (ImGui::MenuItem(label.c_str(), path.c_str()))
                        {
                            to_open = path;
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Clear list"))
                    {
                        recent_files_.clear();
                        Settings cleared;
                        cleared.recent_files = recent_files_;
                        save_settings(cleared);
                    }
                    ImGui::EndMenu();
                    if (not to_open.empty())
                    {
                        load_file(to_open);
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
                if (ImGui::MenuItem("Load node types..."))
                {
                    auto picked = pfd::open_file(
                        "Load node types JSON",
                        dialog_start_dir(doc.loaded_path),
                        { "Node packs", "*.json", "All files", "*" }).result();
                    if (not picked.empty())
                    {
                        NodePackLoadResult const r =
                            load_node_pack(picked.front(), registry_);
                        if (r.added > 0)
                        {
                            push_toast(ToastLevel::Info,
                                       "Loaded " + std::to_string(r.added)
                                           + " node type(s)");
                            refresh_after_pack_load();
                        }
                        if (r.skipped > 0)
                        {
                            push_toast(ToastLevel::Warn,
                                       std::to_string(r.skipped)
                                           + " duplicate type(s) skipped");
                        }
                        for (auto const& w : r.warnings)
                        {
                            push_toast(ToastLevel::Warn, w);
                        }
                        for (auto const& e : r.errors)
                        {
                            push_toast(ToastLevel::Error, e);
                        }
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
                    request_quit();
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit"))
            {
                Document* d = active();
                bool const have_sel =
                    d != nullptr and d->editor.selection_ids().size() >= 2;
                if (ImGui::BeginMenu("Align", have_sel))
                {
                    if (ImGui::MenuItem("Left"))                { align_selection(*d, AlignMode::Left);    }
                    if (ImGui::MenuItem("Right"))               { align_selection(*d, AlignMode::Right);   }
                    if (ImGui::MenuItem("Top"))                 { align_selection(*d, AlignMode::Top);     }
                    if (ImGui::MenuItem("Bottom"))              { align_selection(*d, AlignMode::Bottom);  }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Center horizontally")) { align_selection(*d, AlignMode::CenterH); }
                    if (ImGui::MenuItem("Center vertically"))   { align_selection(*d, AlignMode::CenterV); }
                    ImGui::EndMenu();
                }
                bool const can_distribute =
                    d != nullptr and d->editor.selection_ids().size() >= 3;
                if (ImGui::BeginMenu("Distribute", can_distribute))
                {
                    if (ImGui::MenuItem("Horizontally")) { distribute_selection(*d, true);  }
                    if (ImGui::MenuItem("Vertically"))   { distribute_selection(*d, false); }
                    ImGui::EndMenu();
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
                if (Document* d = active(); d != nullptr)
                {
                    if (ImGui::MenuItem("Fit view", "F"))
                    {
                        d->editor.zoom_to_fit(d->editor.selection_ids());
                    }
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
                if (ImGui::MenuItem("Mini-map", "Ctrl+M", minimap_visible_))
                {
                    minimap_visible_ = not minimap_visible_;
                }
                if (ImGui::MenuItem("Color pins by stage", nullptr,
                                    color_pins_by_stage_))
                {
                    color_pins_by_stage_ = not color_pins_by_stage_;
                    for (auto& d : documents_)
                    {
                        d->adapter.set_color_pins_by_stage(color_pins_by_stage_);
                        d->adapter.rebuild();
                    }
                }
                if (ImGui::MenuItem("Font..."))
                {
                    font_picker_open_ = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("Shortcuts..."))    { shortcuts_open_ = true; }
                if (ImGui::MenuItem("About Piper...")) { about_open_     = true; }
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

            // Confirmation popup for quitting with unsaved changes.
            if (ImGui::BeginPopupModal("##confirm_quit", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                int dirty_count = 0;
                for (auto const& d : documents_)
                {
                    if (d->dirty) { ++dirty_count; }
                }
                char const* plural = "s";
                if (dirty_count == 1)
                {
                    plural = "";
                }
                ImGui::Text("%d document%s with unsaved changes:",
                            dirty_count, plural);
                for (auto const& d : documents_)
                {
                    if (not d->dirty) { continue; }
                    std::string label{"untitled"};
                    if (not d->loaded_path.empty())
                    {
                        label = std::filesystem::path(d->loaded_path).filename().string();
                    }
                    ImGui::BulletText("%s", label.c_str());
                }
                ImGui::Separator();
                if (ImGui::Button("Save all and quit"))
                {
                    bool all_saved = true;
                    for (auto const& d : documents_)
                    {
                        if (not d->dirty) { continue; }
                        std::string target = d->loaded_path;
                        if (target.empty())
                        {
                            // Untitled doc: prompt for a path. Activate
                            // the tab first so the dialog title makes
                            // sense in context.
                            for (int k = 0; k < int(documents_.size()); ++k)
                            {
                                if (documents_[k].get() == d.get())
                                {
                                    active_doc_idx_ = k;
                                    break;
                                }
                            }
                            auto picked = pfd::save_file(
                                "Save Piper file as",
                                dialog_start_dir(d->loaded_path),
                                { "Piper graphs", "*.piper", "All files", "*" }).result();
                            if (picked.empty())
                            {
                                all_saved = false;
                                break;   // user cancelled -> abort the whole quit
                            }
                            target = picked;
                        }
                        if (not save_to(*d, target))
                        {
                            all_saved = false;
                            break;
                        }
                    }
                    if (all_saved)
                    {
                        running_ = false;
                    }
                    quit_confirming_ = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Discard and quit"))
                {
                    running_         = false;
                    quit_confirming_ = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    quit_confirming_ = false;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            // Confirmation popup for closing a dirty document.
            if (ImGui::BeginPopupModal("##confirm_close", nullptr,
                                       ImGuiWindowFlags_AlwaysAutoResize))
            {
                if (confirm_close_idx_ >= 0
                    and confirm_close_idx_ < int(documents_.size()))
                {
                    Document& cd = *documents_[confirm_close_idx_];
                    std::string label{"untitled"};
                    if (not cd.loaded_path.empty())
                    {
                        label = std::filesystem::path(cd.loaded_path).filename().string();
                    }
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
        char const* tb_run_label = "Run##tb_engine_run";
        if (adoc.engine_running)
        {
            tb_run_label = "Stop##tb_engine_run";
        }
        if (ImGui::Button(tb_run_label))
        {
            toggle_engine_run(adoc);
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
                if (adoc.engine != nullptr)
                {
                    adoc.engine->set_mode(adoc.active_mode_profile);
                }
            }
            for (auto const& mp : adoc.graph.mode_profiles())
            {
                bool const sel = (mp.name == adoc.active_mode_profile);
                if (ImGui::Selectable(mp.name.c_str(), sel) and not sel)
                {
                    adoc.active_mode_profile = mp.name;
                    adoc.adapter.set_active_mode_profile(adoc.active_mode_profile);
                    adoc.adapter.rebuild();
                    adoc.lint_dirty = true;
                    if (adoc.engine != nullptr)
                    {
                        adoc.engine->set_mode(adoc.active_mode_profile);
                    }
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
        if (minimap_visible_)
        {
            draw_minimap(adoc, canvas_layout_, dpi_scale_);
        }
        {
            canvas::NodeId const hovered = adoc.editor.hovered_node();
            Node const*     hn  = nullptr;
            NodeType const* hnt = nullptr;
            if (hovered.v != 0)
            {
                hn = adoc.graph.find_node(NodeId(hovered.v));
                if (hn != nullptr)
                {
                    hnt = registry_.find(hn->type);
                }
            }

            // Label cluster hover-highlight: when the cursor is over a
            // Label, outline every other Label that shares its `name`.
            if (hovered.v != 0)
            {
                Label const* hovered_label = adoc.graph.find_label(hovered.v);
                if (hovered_label != nullptr and not hovered_label->name.empty())
                {
                    std::string const target_name = hovered_label->name;
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    for (auto const& cn : adoc.adapter.nodes())
                    {
                        Label const* sibling = adoc.graph.find_label(cn.id.v);
                        if (sibling == nullptr or sibling->name != target_name)
                        {
                            continue;
                        }
                        canvas::Aabb const a = canvas::node_aabb(cn, canvas_layout_);
                        ImVec2 const tl = adoc.editor.canvas_to_screen(a.min);
                        ImVec2 const br = adoc.editor.canvas_to_screen(a.max);
                        ImU32 const col = IM_COL32(0xFF, 0xC0, 0x40, 0xFF);
                        dl->AddRect(tl, br, col, 6.0f, 0, 2.5f);
                    }
                }
            }

            bool const has_help = hnt != nullptr and not hnt->help.empty();
            bool const has_note = hn != nullptr and not hn->note.empty();
            if (has_help or has_note)
            {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(360.0f);
                if (hn != nullptr)
                {
                    ImGui::TextUnformatted(hn->name.c_str());
                }
                if (has_help)
                {
                    ImGui::TextDisabled("%s", hnt->help.c_str());
                }
                if (has_note)
                {
                    if (has_help)
                    {
                        ImGui::Separator();
                    }
                    ImGui::TextUnformatted(hn->note.c_str());
                }
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
        }
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
                    NodeId selected = invalid_node_id;
                    if (not adoc.selection.empty())
                    {
                        selected = adoc.selection.front();
                    }
                    if (inspector_.draw(adoc.graph, registry_, adoc.command_stack, selected,
                                        theme_, adoc.active_mode_profile))
                    {
                        adoc.dirty = true;
                        adoc.lint_dirty = true;
                        adoc.adapter.rebuild();
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Stages"))
                {
                    if (stages_panel_.draw(adoc.graph, adoc.command_stack, adoc.current_stage))
                    {
                        adoc.dirty = true;
                        adoc.lint_dirty = true;
                        adoc.adapter.set_current_stage(adoc.current_stage);
                        adoc.adapter.rebuild();
                    }
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Modes"))
                {
                    if (modes_panel_.draw(adoc.graph, registry_, adoc.command_stack, theme_, adoc.active_mode_profile))
                    {
                        adoc.dirty = true;
                        adoc.lint_dirty = true;
                        adoc.adapter.set_active_mode_profile(adoc.active_mode_profile);
                        adoc.adapter.rebuild();
                    }
                    ImGui::EndTabItem();
                }
                ImGuiTabItemFlags live_flags = ImGuiTabItemFlags_None;
                if (focus_live_tab_pending_)
                {
                    live_flags = ImGuiTabItemFlags_SetSelected;
                    focus_live_tab_pending_ = false;
                }
                if (ImGui::BeginTabItem("Live", nullptr, live_flags))
                {
                    draw_live_panel(adoc);
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

        // ----- Status bar: path + problems + cursor coords + zoom -----
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

        ImGui::SameLine();
        ImVec2 const mouse    = ImGui::GetMousePos();
        auto   const editor_vp = adoc.editor.viewport();
        bool const inside =
            editor_vp.size_screen.x > 0.0f and editor_vp.size_screen.y > 0.0f
            and mouse.x >= editor_vp.origin_screen.x
            and mouse.x < editor_vp.origin_screen.x + editor_vp.size_screen.x
            and mouse.y >= editor_vp.origin_screen.y
            and mouse.y < editor_vp.origin_screen.y + editor_vp.size_screen.y;
        if (inside)
        {
            ImVec2 const c = adoc.editor.screen_to_canvas(mouse);
            ImGui::TextDisabled("  x: %.1f  y: %.1f", c.x, c.y);
        }
        else
        {
            ImGui::TextDisabled("  x: --   y: --");
        }
        ImGui::SameLine();
        ImGui::TextDisabled("  zoom: %.0f%%", adoc.editor.zoom() * 100.0f);

        ImGui::End();

        // App-level shortcuts. Each is claimed with RouteAlways so
        // ImGui built-ins (menu nav on Alt, word nav on Ctrl, list
        // nav on PageUp/Down, ...) cannot fire alongside us.
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Q,
                            ImGuiInputFlags_RouteAlways))
        {
            request_quit();
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
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_M,
                            ImGuiInputFlags_RouteAlways))
        {
            minimap_visible_ = not minimap_visible_;
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
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F,
                            ImGuiInputFlags_RouteAlways))
        {
            find_open_     = true;
            find_focus_    = true;
            find_buf_.clear();
            find_selected_ = 0;
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

        if (font_picker_open_)
        {
            ImGui::OpenPopup("##font_picker");
            font_picker_open_ = false;
            if (not system_fonts_scanned_)
            {
                system_fonts_         = scan_system_fonts();
                system_fonts_scanned_ = true;
            }
            font_path_buf_     = theme_.font_path;
            font_pending_size_ = theme_.font_size;
            font_filter_buf_.clear();
        }
        if (ImGui::BeginPopup("##font_picker"))
        {
            ImGui::TextUnformatted("Font");
            ImGui::Separator();

            ImGui::SetNextItemWidth(360.0f);
            ImGui::InputTextWithHint("filter", "search...", &font_filter_buf_);

            ImVec2 const list_size{ 480.0f, 240.0f };
            if (ImGui::BeginListBox("##font_list", list_size))
            {
                for (auto const& bf : piper::studio::bundled_fonts())
                {
                    std::string const path = std::string{"bundled:"} + bf.name;
                    if (not font_filter_buf_.empty()
                        and path.find(font_filter_buf_) == std::string::npos)
                    {
                        continue;
                    }
                    std::string const label = std::string{"(bundled) "} + bf.name;
                    bool const sel = (path == font_path_buf_);
                    ImGui::PushID(path.c_str());
                    if (ImGui::Selectable(label.c_str(), sel)) { font_path_buf_ = path; }
                    ImGui::PopID();
                }
                for (auto const& path : system_fonts_)
                {
                    if (not font_filter_buf_.empty()
                        and path.find(font_filter_buf_) == std::string::npos)
                    {
                        continue;
                    }
                    std::string const label = std::filesystem::path(path).filename().string();
                    bool const sel = (path == font_path_buf_);
                    ImGui::PushID(path.c_str());
                    if (ImGui::Selectable(label.c_str(), sel)) { font_path_buf_ = path; }
                    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", path.c_str()); }
                    ImGui::PopID();
                }
                ImGui::EndListBox();
            }

            ImGui::SetNextItemWidth(360.0f);
            ImGui::InputText("path", &font_path_buf_);
            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                auto picked = pfd::open_file(
                    "Select font",
                    dialog_start_dir(font_path_buf_),
                    { "Fonts", "*.ttf *.otf *.TTF *.OTF", "All files", "*" }).result();
                if (not picked.empty()) { font_path_buf_ = picked.front(); }
            }
            ImGui::TextDisabled("Empty path = ImGui built-in");

            ImGui::SetNextItemWidth(120.0f);
            ImGui::DragFloat("size", &font_pending_size_, 0.5f, 8.0f, 48.0f, "%.1f px");

            ImGui::Separator();
            if (ImGui::Button("Apply"))
            {
                if (font_path_buf_ != theme_.font_path
                    or font_pending_size_ != theme_.font_size)
                {
                    theme_.font_path   = font_path_buf_;
                    theme_.font_size   = font_pending_size_;
                    wants_font_reload_ = true;
                    Settings s;
                    s.font_path = theme_.font_path;
                    s.font_size = theme_.font_size;
                    save_settings(s);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Use default"))
            {
                if (not theme_.font_path.empty())
                {
                    theme_.font_path.clear();
                    wants_font_reload_ = true;
                    Settings s;
                    s.font_path = theme_.font_path;
                    s.font_size = theme_.font_size;
                    save_settings(s);
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (shortcuts_open_)
        {
            ImGui::OpenPopup("##shortcuts");
            shortcuts_open_ = false;
        }
        if (ImGui::BeginPopup("##shortcuts"))
        {
            struct ShortcutRow { char const* keys; char const* desc; };
            ShortcutRow const file_rows[] = {
                { "Ctrl+S",        "Save"               },
                { "Ctrl+T",        "New tab"            },
                { "Ctrl+W",        "Close tab"          },
                { "Ctrl+Tab",      "Switch tab"         },
                { "Ctrl+Q",        "Quit"               },
            };
            ShortcutRow const edit_rows[] = {
                { "Ctrl+Z",        "Undo"               },
                { "Ctrl+Shift+Z",  "Redo"               },
                { "Ctrl+C",        "Copy selection"     },
                { "Ctrl+V",        "Paste"              },
                { "Ctrl+X",        "Cut selection"      },
                { "Ctrl+D",        "Duplicate selection"},
                { "Delete",        "Delete selection"   },
            };
            ShortcutRow const view_rows[] = {
                { "F",             "Fit view to selection or all" },
                { "Ctrl+B",        "Toggle right panel" },
                { "Ctrl+G",        "Toggle snap to grid"},
                { "Ctrl+M",        "Toggle mini-map"    },
                { "Ctrl+F",        "Find node"          },
            };
            ShortcutRow const stage_rows[] = {
                { "Alt+Left",      "Previous stage"     },
                { "Alt+Right",     "Next stage"         },
            };

            auto render_section = [](char const* title,
                                     ShortcutRow const* rows,
                                     std::size_t n)
            {
                ImGui::TextDisabled("%s", title);
                for (std::size_t i = 0; i < n; ++i)
                {
                    ImGui::Text("  %-14s  %s", rows[i].keys, rows[i].desc);
                }
                ImGui::Spacing();
            };
            render_section("File",  file_rows,  sizeof(file_rows)  / sizeof(ShortcutRow));
            render_section("Edit",  edit_rows,  sizeof(edit_rows)  / sizeof(ShortcutRow));
            render_section("View",  view_rows,  sizeof(view_rows)  / sizeof(ShortcutRow));
            render_section("Stage", stage_rows, sizeof(stage_rows) / sizeof(ShortcutRow));

            ImGui::Separator();
            if (ImGui::Button("Close"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (find_open_)
        {
            ImGui::OpenPopup("##find");
            find_open_ = false;
        }
        if (ImGui::BeginPopup("##find"))
        {
            ImGui::SetNextItemWidth(360.0f);
            if (find_focus_)
            {
                ImGui::SetKeyboardFocusHere();
                find_focus_ = false;
            }
            bool const enter = ImGui::InputText(
                "##find_input", &find_buf_,
                ImGuiInputTextFlags_EnterReturnsTrue);

            std::vector<NodeId> matches;
            for (auto const& n : adoc.graph.nodes())
            {
                if (find_buf_.empty()
                    or n.name.find(find_buf_) != std::string::npos)
                {
                    matches.push_back(n.id);
                }
            }
            if (find_selected_ >= int(matches.size()))
            {
                find_selected_ = 0;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)
                and not matches.empty())
            {
                find_selected_ = (find_selected_ + 1) % int(matches.size());
            }
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)
                and not matches.empty())
            {
                find_selected_ = (find_selected_ - 1 + int(matches.size())) % int(matches.size());
            }

            ImVec2 const list_size{ 360.0f, 240.0f };
            if (ImGui::BeginListBox("##find_results", list_size))
            {
                for (int i = 0; i < int(matches.size()); ++i)
                {
                    Node const* n = adoc.graph.find_node(matches[i]);
                    if (n == nullptr)
                    {
                        continue;
                    }
                    ImGui::PushID(i);
                    bool const sel = (i == find_selected_);
                    if (ImGui::Selectable(n->name.c_str(), sel,
                                          ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        find_selected_ = i;
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                        {
                            std::array<canvas::NodeId, 1> ids{ canvas::NodeId{ n->id } };
                            adoc.editor.set_selection(ids);
                            adoc.editor.scroll_to(canvas::NodeId{ n->id });
                            ImGui::CloseCurrentPopup();
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndListBox();
            }
            ImGui::TextDisabled("%d match(es)", int(matches.size()));

            if (enter and not matches.empty())
            {
                Node const* n = adoc.graph.find_node(matches[find_selected_]);
                if (n != nullptr)
                {
                    std::array<canvas::NodeId, 1> ids{ canvas::NodeId{ n->id } };
                    adoc.editor.set_selection(ids);
                    adoc.editor.scroll_to(canvas::NodeId{ n->id });
                }
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (about_open_)
        {
            ImGui::OpenPopup("##about");
            about_open_ = false;
        }
        if (ImGui::BeginPopup("##about"))
        {
            ImGui::TextUnformatted("Piper");
            ImGui::TextDisabled("Node-graph editor");
            ImGui::Separator();

            ImGui::TextUnformatted("License");
            ImGui::BeginChild("##project_license_text",
                              ImVec2{ 720.0f, 200.0f },
                              ImGuiChildFlags_Borders);
            ImGui::TextUnformatted(piper::studio::project_license());
            ImGui::EndChild();

            ImGui::Separator();
            ImGui::TextUnformatted("Bundled assets");

            auto licenses = piper::studio::bundled_licenses();
            if (licenses.empty())
            {
                ImGui::TextDisabled("(none)");
            }
            else
            {
                if (about_selected_ >= int(licenses.size()))
                {
                    about_selected_ = 0;
                }
                if (ImGui::BeginListBox("##license_names", ImVec2{ 220.0f, 120.0f }))
                {
                    for (int i = 0; i < int(licenses.size()); ++i)
                    {
                        bool const sel = (i == about_selected_);
                        if (ImGui::Selectable(licenses[i].name, sel))
                        {
                            about_selected_ = i;
                        }
                    }
                    ImGui::EndListBox();
                }
                ImGui::SameLine();
                ImGui::BeginChild("##license_text",
                                  ImVec2{ 480.0f, 200.0f },
                                  ImGuiChildFlags_Borders);
                ImGui::TextUnformatted(licenses[about_selected_].text);
                ImGui::EndChild();
            }
            ImGui::Separator();
            if (ImGui::Button("Close"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (autosave_recovery_open_)
        {
            ImGui::OpenPopup("##autosave_recovery");
            autosave_recovery_open_ = false;
        }
        if (ImGui::BeginPopupModal("##autosave_recovery", nullptr,
                                    ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Autosave recovery");
            ImGui::TextDisabled(
                "%zu autosaved document(s) from a previous session.",
                autosave_pending_.size());
            ImGui::Separator();
            std::string to_open;
            std::string to_discard;
            for (auto const& path : autosave_pending_)
            {
                ImGui::PushID(path.c_str());
                ImGui::TextUnformatted(
                    std::filesystem::path(path).filename().string().c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Open"))
                {
                    to_open = path;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Discard"))
                {
                    to_discard = path;
                }
                ImGui::PopID();
            }
            ImGui::Separator();
            if (ImGui::Button("Discard all"))
            {
                for (auto const& p : autosave_pending_)
                {
                    std::error_code ec;
                    std::filesystem::remove(p, ec);
                }
                autosave_pending_.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Close"))
            {
                ImGui::CloseCurrentPopup();
            }
            if (not to_open.empty())
            {
                std::string const fname =
                    std::filesystem::path(to_open).filename().string();
                if (load_file(to_open))
                {
                    push_toast(ToastLevel::Info, "Recovered " + fname);
                    // Only drop the autosave once its contents are safely
                    // loaded -- a failed load must keep the sole copy.
                    std::error_code ec;
                    std::filesystem::remove(to_open, ec);
                    autosave_pending_.erase(
                        std::remove(autosave_pending_.begin(), autosave_pending_.end(), to_open),
                        autosave_pending_.end());
                    if (autosave_pending_.empty())
                    {
                        ImGui::CloseCurrentPopup();
                    }
                }
                else
                {
                    push_toast(ToastLevel::Error,
                               "Could not recover " + fname + " -- autosave kept");
                }
            }
            if (not to_discard.empty())
            {
                std::error_code ec;
                std::filesystem::remove(to_discard, ec);
                autosave_pending_.erase(
                    std::remove(autosave_pending_.begin(), autosave_pending_.end(), to_discard),
                    autosave_pending_.end());
                if (autosave_pending_.empty())
                {
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }

        // Sweep popups left open on docs that are no longer active.
        // Switching tabs (or closing one) would otherwise leave a doc
        // with editing/dragging state that, on next visit, opens
        // popups against stale ids -- or worse, lets a frame in the
        // outgoing tab corrupt the incoming doc through ImGui's
        // global popup state. This pass coalesces any pending text
        // edit, reverts any abandoned drag, and clears the flags.
        // Iterate by index against a captured size so a future
        // doc-add inside the loop body doesn't invalidate references.
        std::size_t const sweep_count = documents_.size();
        for (std::size_t doc_i = 0; doc_i < sweep_count; ++doc_i)
        {
            std::unique_ptr<Document>& d = documents_[doc_i];
            if (d.get() == &adoc) { continue; }
            PopupState& p = d->popup;
            if (p.editing_annotation != invalid_annotation_id)
            {
                Annotation* a = d->graph.find_annotation_mut(p.editing_annotation);
                if (a != nullptr and a->text != p.anno_original_text)
                {
                    std::string const final_text = a->text;
                    a->text = p.anno_original_text;
                    d->command_stack.push(std::make_unique<SetAnnotationTextCommand>(
                            p.editing_annotation, final_text), d->graph);
                    d->dirty      = true;
                    d->lint_dirty = true;
                }
                if (p.annotation_group_open)
                {
                    d->command_stack.close_group();
                    p.annotation_group_open = false;
                }
                p.editing_annotation = invalid_annotation_id;
                p.annotation_buf_id  = invalid_annotation_id;
            }
            if (p.editing_node != invalid_node_id)
            {
                // Symmetric with the active-doc click-outside path:
                // commit any typed name before clearing the popup so
                // tab-switching doesn't silently drop user input.
                piper::Node const* n = d->graph.find_node(p.editing_node);
                if (n != nullptr and p.node_name_buf != n->name)
                {
                    d->command_stack.push(std::make_unique<RenameNodeCommand>(
                        p.editing_node, p.node_name_buf), d->graph);
                    d->dirty      = true;
                    d->lint_dirty = true;
                    d->adapter.rebuild();
                }
                p.editing_node     = invalid_node_id;
                p.node_name_buf_id = invalid_node_id;
            }
            if (p.editing_label != invalid_label_id)
            {
                Label const* l = d->graph.find_label(p.editing_label);
                if (l != nullptr and p.label_name_buf != l->name)
                {
                    d->command_stack.push(std::make_unique<SetLabelNameCommand>(
                        p.editing_label, p.label_name_buf), d->graph);
                    d->dirty      = true;
                    d->lint_dirty = true;
                    d->adapter.rebuild();
                }
                if (p.label_group_open)
                {
                    d->command_stack.close_group();
                    p.label_group_open = false;
                }
                p.editing_label     = invalid_label_id;
                p.label_name_buf_id = invalid_label_id;
            }
            if (p.dragging_annotation != invalid_annotation_id)
            {
                // Drag abandoned mid-flight (no live mouse on this
                // doc); revert to start position and clear.
                Annotation* a = d->graph.find_annotation_mut(p.dragging_annotation);
                if (a != nullptr) { a->pos = p.annotation_drag_start_pos; }
                p.dragging_annotation = invalid_annotation_id;
            }
        }

        // Live-edit popup: every keystroke writes Annotation::text
        // directly so the canvas updates per stroke. On close we
        // restore the captured original and push ONE command
        // (original -> final) so the editing session collapses to a
        // single undo step.
        char const* const anno_popup_id = "##edit_annotation";
        if (adoc.popup.editing_annotation != invalid_annotation_id
            and not ImGui::IsPopupOpen(anno_popup_id))
        {
            ImGui::OpenPopup(anno_popup_id);
        }
        auto coalesce_annotation_text = [&]()
        {
            Annotation* a = adoc.graph.find_annotation_mut(adoc.popup.editing_annotation);
            if (a == nullptr) { return; }
            if (a->text == adoc.popup.anno_original_text) { return; }
            std::string const final_text = a->text;
            a->text = adoc.popup.anno_original_text;
            adoc.command_stack.push(std::make_unique<SetAnnotationTextCommand>(
                            adoc.popup.editing_annotation, final_text), adoc.graph);
            adoc.dirty      = true;
            adoc.lint_dirty = true;
        };
        auto close_annotation_popup = [&]()
        {
            // Close the per-popup undo group. CommandStack folds the
            // accumulated text/color/pos/size mutations into a single
            // CompositeCommand so the popup undoes in one step. Each
            // command kind also merges within itself (try_merge), so
            // a slider drag of N frames stays one entry.
            if (adoc.popup.annotation_group_open)
            {
                adoc.command_stack.close_group();
                adoc.popup.annotation_group_open = false;
            }
            adoc.popup.editing_annotation = invalid_annotation_id;
            adoc.popup.annotation_buf_id  = invalid_annotation_id;
            ImGui::CloseCurrentPopup();
        };
        if (ImGui::BeginPopup(anno_popup_id))
        {
            Annotation* a = adoc.graph.find_annotation_mut(adoc.popup.editing_annotation);
            if (a == nullptr)
            {
                close_annotation_popup();
            }
            else
            {
                ImGui::TextUnformatted("Edit annotation  (Esc to close)");
                ImGui::Separator();

                if (adoc.popup.annotation_buf_id != adoc.popup.editing_annotation)
                {
                    adoc.popup.annotation_buf_id   = adoc.popup.editing_annotation;
                    adoc.popup.anno_original_text  = a->text;
                    adoc.popup.annotation_text_buf = a->text;
                    ImGui::SetKeyboardFocusHere();
                    if (not adoc.popup.annotation_group_open)
                    {
                        adoc.command_stack.open_group();
                        adoc.popup.annotation_group_open = true;
                    }
                }

                // ImGui's InputText reverts its buffer to the initial
                // value when the user presses Escape. Sample the key
                // BEFORE drawing the input so we know whether to skip
                // the buffer->text mirror this frame; otherwise we'd
                // overwrite the user's typed-up state with the revert.
                bool const esc_now =
                    ImGui::IsKeyPressed(ImGuiKey_Escape, false);

                ImVec2 const text_size{ 360.0f, ImGui::GetTextLineHeight() * 5.0f };
                ImGui::InputTextMultiline("##anno_text",
                                           &adoc.popup.annotation_text_buf,
                                           text_size);
                // Mirror the buffer into the live Annotation each
                // frame so the canvas reflects typing immediately.
                if (not esc_now and adoc.popup.annotation_text_buf != a->text)
                {
                    a->text    = adoc.popup.annotation_text_buf;
                    adoc.dirty = true;
                }

                float col[4] = {
                    float(a->color.r()) / 255.0f,
                    float(a->color.g()) / 255.0f,
                    float(a->color.b()) / 255.0f,
                    float(a->color.a()) / 255.0f,
                };
                ImGui::SetNextItemWidth(360.0f);
                if (ImGui::ColorEdit4("color", col))
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
                    if (new_c != a->color)
                    {
                        adoc.command_stack.push(std::make_unique<SetAnnotationColorCommand>(
                            adoc.popup.editing_annotation, new_c), adoc.graph);
                        adoc.dirty = true;
                    }
                }

                float pos[2] = { a->pos.x, a->pos.y };
                ImGui::SetNextItemWidth(360.0f);
                if (ImGui::DragFloat2("pos", pos, 1.0f))
                {
                    Point const np{ pos[0], pos[1] };
                    if (np != a->pos)
                    {
                        adoc.command_stack.push(std::make_unique<SetAnnotationPosCommand>(
                            adoc.popup.editing_annotation, np), adoc.graph);
                        adoc.dirty = true;
                    }
                }

                float size[2] = { a->size.x, a->size.y };
                ImGui::SetNextItemWidth(360.0f);
                if (ImGui::DragFloat2("size", size, 1.0f, 8.0f, 4096.0f))
                {
                    Point const ns{ size[0], size[1] };
                    if (ns != a->size)
                    {
                        adoc.command_stack.push(std::make_unique<SetAnnotationSizeCommand>(
                            adoc.popup.editing_annotation, ns), adoc.graph);
                        adoc.dirty = true;
                    }
                }

                if (esc_now)
                {
                    coalesce_annotation_text();
                    close_annotation_popup();
                }
            }
            ImGui::EndPopup();
        }
        else if (adoc.popup.editing_annotation != invalid_annotation_id)
        {
            // Popup got dismissed externally (click-outside). ImGui
            // already closed the popup, so close_annotation_popup's
            // CloseCurrentPopup is a no-op; the helper still runs the
            // group close + flag clear consistently with all other
            // close paths.
            coalesce_annotation_text();
            close_annotation_popup();
        }

        char const* const node_popup_id = "##rename_node";
        if (adoc.popup.editing_node != invalid_node_id
            and not ImGui::IsPopupOpen(node_popup_id))
        {
            ImGui::OpenPopup(node_popup_id);
        }
        auto commit_node_rename = [&]()
        {
            piper::Node const* n = adoc.graph.find_node(adoc.popup.editing_node);
            if (n == nullptr) { return; }
            if (adoc.popup.node_name_buf == n->name) { return; }
            adoc.command_stack.push(std::make_unique<RenameNodeCommand>(
                            adoc.popup.editing_node, adoc.popup.node_name_buf), adoc.graph);
            adoc.dirty      = true;
            adoc.lint_dirty = true;
            adoc.adapter.rebuild();
        };
        auto close_node_popup = [&]()
        {
            adoc.popup.editing_node     = invalid_node_id;
            adoc.popup.node_name_buf_id = invalid_node_id;
            ImGui::CloseCurrentPopup();
        };
        if (ImGui::BeginPopup(node_popup_id))
        {
            piper::Node const* n = adoc.graph.find_node(adoc.popup.editing_node);
            if (n == nullptr)
            {
                close_node_popup();
            }
            else
            {
                if (adoc.popup.node_name_buf_id != adoc.popup.editing_node)
                {
                    adoc.popup.node_name_buf_id = adoc.popup.editing_node;
                    adoc.popup.node_name_buf    = n->name;
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::TextUnformatted("Rename node");
                ImGui::Separator();
                ImGui::SetNextItemWidth(280.0f);
                bool const submit = ImGui::InputText(
                    "##node_name", &adoc.popup.node_name_buf,
                    ImGuiInputTextFlags_EnterReturnsTrue);
                if (submit)
                {
                    commit_node_rename();
                    close_node_popup();
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                {
                    close_node_popup();
                }
            }
            ImGui::EndPopup();
        }
        else if (adoc.popup.editing_node != invalid_node_id)
        {
            // Click-outside dismiss: persist the typed name.
            commit_node_rename();
            close_node_popup();
        }

        char const* const label_popup_id = "##rename_label";
        if (adoc.popup.editing_label != invalid_label_id
            and not ImGui::IsPopupOpen(label_popup_id))
        {
            ImGui::OpenPopup(label_popup_id);
        }
        auto commit_label_rename = [&]()
        {
            Label const* l = adoc.graph.find_label(adoc.popup.editing_label);
            if (l == nullptr) { return; }
            if (adoc.popup.label_name_buf == l->name) { return; }
            adoc.command_stack.push(std::make_unique<SetLabelNameCommand>(
                            adoc.popup.editing_label, adoc.popup.label_name_buf), adoc.graph);
            adoc.dirty      = true;
            adoc.lint_dirty = true;
            adoc.adapter.rebuild();
        };
        auto close_label_popup = [&]()
        {
            if (adoc.popup.label_group_open)
            {
                adoc.command_stack.close_group();
                adoc.popup.label_group_open = false;
            }
            adoc.popup.editing_label     = invalid_label_id;
            adoc.popup.label_name_buf_id = invalid_label_id;
            ImGui::CloseCurrentPopup();
        };
        if (ImGui::BeginPopup(label_popup_id))
        {
            Label const* l = adoc.graph.find_label(adoc.popup.editing_label);
            if (l == nullptr)
            {
                close_label_popup();
            }
            else
            {
                if (adoc.popup.label_name_buf_id != adoc.popup.editing_label)
                {
                    adoc.popup.label_name_buf_id = adoc.popup.editing_label;
                    adoc.popup.label_name_buf    = l->name;
                    ImGui::SetKeyboardFocusHere();
                    if (not adoc.popup.label_group_open)
                    {
                        adoc.command_stack.open_group();
                        adoc.popup.label_group_open = true;
                    }
                }
                ImGui::TextUnformatted("Edit label  (Enter saves, Esc cancels)");
                ImGui::Separator();
                ImGui::TextUnformatted("Name");
                ImGui::SameLine(80.0f);
                ImGui::SetNextItemWidth(280.0f);
                bool const submit = ImGui::InputText(
                    "##label_name", &adoc.popup.label_name_buf,
                    ImGuiInputTextFlags_EnterReturnsTrue);

                float col[4] = {
                    float(l->color.r()) / 255.0f,
                    float(l->color.g()) / 255.0f,
                    float(l->color.b()) / 255.0f,
                    float(l->color.a()) / 255.0f,
                };
                ImGui::TextUnformatted("Color");
                ImGui::SameLine(80.0f);
                ImGui::SetNextItemWidth(280.0f);
                if (ImGui::ColorEdit4("##label_color", col,
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
                    if (new_c != l->color)
                    {
                        adoc.command_stack.push(std::make_unique<SetLabelColorCommand>(
                            adoc.popup.editing_label, new_c), adoc.graph);
                        adoc.dirty = true;
                        adoc.adapter.rebuild();
                    }
                }

                // Submit only fires on the name field's Enter; Esc
                // closes regardless of which widget has focus.
                if (submit)
                {
                    commit_label_rename();
                    close_label_popup();
                }
                else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                {
                    close_label_popup();
                }
            }
            ImGui::EndPopup();
        }
        else if (adoc.popup.editing_label != invalid_label_id)
        {
            // Click-outside dismiss: persist the typed name.
            commit_label_rename();
            close_label_popup();
        }

        // ----- Toast stack (bottom-right) -----
        {
            constexpr auto ttl   = std::chrono::milliseconds{3500};
            constexpr auto fade  = std::chrono::milliseconds{600};
            auto const now       = std::chrono::steady_clock::now();
            toasts_.erase(
                std::remove_if(toasts_.begin(), toasts_.end(),
                               [&](Toast const& t)
                               {
                                   return now - t.spawned > ttl;
                               }),
                toasts_.end());

            ImGuiViewport const* toast_vp = ImGui::GetMainViewport();
            float const margin = 12.0f;
            float       y      = toast_vp->WorkPos.y + toast_vp->WorkSize.y - margin;
            for (std::size_t i = 0; i < toasts_.size(); ++i)
            {
                Toast const& t   = toasts_[toasts_.size() - 1 - i];
                auto const   age = now - t.spawned;
                float        alpha = 1.0f;
                if (age > ttl - fade)
                {
                    auto const remaining = ttl - age;
                    alpha = float(remaining.count()) / float(fade.count());
                    if (alpha < 0.0f) { alpha = 0.0f; }
                }

                ImVec4 bg{ 0.18f, 0.18f, 0.18f, 0.95f * alpha };
                if (t.level == ToastLevel::Warn)
                {
                    bg = ImVec4{ 0.40f, 0.30f, 0.10f, 0.95f * alpha };
                }
                else if (t.level == ToastLevel::Error)
                {
                    bg = ImVec4{ 0.45f, 0.15f, 0.15f, 0.95f * alpha };
                }

                ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
                std::string const id = "##toast_" + std::to_string(i);
                ImGui::SetNextWindowBgAlpha(0.95f * alpha);
                ImGui::SetNextWindowPos(ImVec2{ toast_vp->WorkPos.x + toast_vp->WorkSize.x - margin, y },
                                        ImGuiCond_Always, ImVec2{ 1.0f, 1.0f });
                ImGui::Begin(id.c_str(), nullptr,
                             ImGuiWindowFlags_NoDecoration
                             | ImGuiWindowFlags_AlwaysAutoResize
                             | ImGuiWindowFlags_NoSavedSettings
                             | ImGuiWindowFlags_NoFocusOnAppearing
                             | ImGuiWindowFlags_NoNav
                             | ImGuiWindowFlags_NoInputs);
                ImGui::TextUnformatted(t.message.c_str());
                ImVec2 const sz = ImGui::GetWindowSize();
                ImGui::End();
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();
                y -= sz.y + 6.0f;
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
                case canvas::Event::NodeMoved:
                case canvas::Event::NodeDeleted:
                case canvas::Event::LinkCreated:
                case canvas::Event::PasteRequested:
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
                case canvas::Event::SelectionChanged:
                {
                    doc.selection.clear();
                    doc.selection.reserve(ev.selection.size());
                    for (auto const& cid : ev.selection)
                    {
                        doc.selection.push_back(NodeId(cid.v));
                    }
                    break;
                }
                case canvas::Event::NodeMoved:
                {
                    NodeId const id = NodeId(ev.node.v);
                    Point  const new_pos{ ev.pos.x, ev.pos.y };
                    // Labels share the NodeId space; route them to
                    // MoveLabelCommand since MoveNodeCommand silently
                    // no-ops on label IDs.
                    if (doc.graph.find_label(id) != nullptr)
                    {
                        doc.command_stack.push(std::make_unique<MoveLabelCommand>(id, new_pos), doc.graph);
                    }
                    else
                    {
                        doc.command_stack.push(std::make_unique<MoveNodeCommand>(id, new_pos), doc.graph);
                    }
                    doc.dirty     = true;
                    // NodeMoved doesn't change topology so lints are
                    // unaffected; flag is intentionally NOT set here.
                    dirty_rebuild = true;
                    break;
                }
                case canvas::Event::NodeDeleted:
                {
                    NodeId const id = NodeId(ev.node.v);
                    if (doc.graph.find_label(id) != nullptr)
                    {
                        doc.command_stack.push(std::make_unique<DeleteLabelCommand>(id), doc.graph);
                    }
                    else
                    {
                        doc.command_stack.push(std::make_unique<DeleteNodeCommand>(id), doc.graph);
                    }
                    doc.dirty      = true;
                    doc.lint_dirty = true;
                    dirty_rebuild  = true;
                    break;
                }
                case canvas::Event::LinkCreated:
                {
                    PinRef const from = doc.adapter.pin_id_to_ref(ev.pin_from);
                    PinRef const to   = doc.adapter.pin_id_to_ref(ev.pin_to);
                    if (from.attr.empty() or to.attr.empty())
                    {
                        break;
                    }
                    // Authoritative domain check for node<->node links
                    // (same-node, kind/type mismatch, fan-in). Label
                    // endpoints resolve type/direction at build via the
                    // label cluster, so skip the node-only validator there.
                    bool const involves_label =
                        doc.graph.find_label(from.node) != nullptr
                        or doc.graph.find_label(to.node) != nullptr;
                    if (not involves_label)
                    {
                        piper::TypeCheck const tc;
                        if (piper::validate_connection(doc.graph, from, to, tc)
                            != piper::Connect::Allow)
                        {
                            break;
                        }
                    }
                    // Either endpoint may be a label (wildcard pin); the
                    // declared data_type comes from whichever side is a
                    // real Node attribute.
                    std::string data_type;
                    if (Node const* fn = doc.graph.find_node(from.node); fn != nullptr)
                    {
                        if (Attribute const* a = fn->find_attr(from.attr); a != nullptr)
                        {
                            data_type = a->data_type;
                        }
                    }
                    if (data_type.empty())
                    {
                        if (Node const* tn = doc.graph.find_node(to.node); tn != nullptr)
                        {
                            if (Attribute const* a = tn->find_attr(to.attr); a != nullptr)
                            {
                                data_type = a->data_type;
                            }
                        }
                    }
                    doc.command_stack.push(std::make_unique<CreateLinkCommand>(from, to, data_type), doc.graph);
                    doc.dirty      = true;
                    doc.lint_dirty = true;
                    dirty_rebuild  = true;
                    break;
                }
                case canvas::Event::LinkDeleted:
                {
                    doc.command_stack.push(std::make_unique<DeleteLinkCommand>(LinkId(ev.link.v)), doc.graph);
                    doc.dirty      = true;
                    doc.lint_dirty = true;
                    dirty_rebuild  = true;
                    break;
                }
                case canvas::Event::PinSideToggled:
                {
                    PinRef const ref = doc.adapter.pin_id_to_ref(ev.pin_from);
                    if (ref.attr.empty())
                    {
                        break;
                    }
                    bool cur = false;
                    if (Node const* n = doc.graph.find_node(ref.node); n != nullptr)
                    {
                        if (Attribute const* a = n->find_attr(ref.attr); a != nullptr)
                        {
                            cur = a->flip_side;
                        }
                    }
                    doc.command_stack.push(
                        std::make_unique<SetPinFlipSideCommand>(ref.node, ref.attr, not cur),
                        doc.graph);
                    doc.dirty     = true;
                    dirty_rebuild = true;
                    break;
                }
                case canvas::Event::CopyRequested:
                {
                    copy_to_clipboard(doc, ev.selection);
                    break;
                }
                case canvas::Event::PasteRequested:
                {
                    doc.command_stack.open_group();
                    if (paste_from_clipboard(doc, ev.pos))
                    {
                        doc.dirty      = true;
                        doc.lint_dirty = true;
                        dirty_rebuild  = true;
                    }
                    doc.command_stack.close_group();
                    break;
                }
                case canvas::Event::CutRequested:
                {
                    if (ev.selection.empty())
                    {
                        break;
                    }
                    copy_to_clipboard(doc, ev.selection);
                    doc.command_stack.open_group();
                    for (canvas::NodeId const cn : ev.selection)
                    {
                        NodeId const id = NodeId(cn.v);
                        // Labels share the NodeId space; route them to
                        // DeleteLabelCommand or DeleteNodeCommand
                        // silently no-ops on label ids.
                        if (doc.graph.find_label(id) != nullptr)
                        {
                            doc.command_stack.push(std::make_unique<DeleteLabelCommand>(id), doc.graph);
                        }
                        else
                        {
                            doc.command_stack.push(std::make_unique<DeleteNodeCommand>(id), doc.graph);
                        }
                    }
                    doc.command_stack.close_group();
                    doc.dirty      = true;
                    doc.lint_dirty = true;
                    dirty_rebuild  = true;
                    break;
                }
                case canvas::Event::DuplicateRequested:
                {
                    if (ev.selection.empty())
                    {
                        break;
                    }
                    copy_to_clipboard(doc, ev.selection);
                    // Anchor the duplicates at +offset from the originals
                    // (a tiny diagonal nudge), not at the cursor — Ctrl+D
                    // should not teleport a selection across the canvas.
                    ImVec2 const anchor{
                        clipboard_.origin.x + 20.0f,
                        clipboard_.origin.y + 20.0f,
                    };
                    doc.command_stack.open_group();
                    if (paste_from_clipboard(doc, anchor))
                    {
                        doc.dirty      = true;
                        doc.lint_dirty = true;
                        dirty_rebuild  = true;
                    }
                    doc.command_stack.close_group();
                    break;
                }
                case canvas::Event::UndoRequested:
                {
                    if (doc.command_stack.can_undo())
                    {
                        doc.command_stack.undo(doc.graph);
                        doc.dirty      = true;
                        doc.lint_dirty = true;
                        dirty_rebuild  = true;
                    }
                    break;
                }
                case canvas::Event::RedoRequested:
                {
                    if (doc.command_stack.can_redo())
                    {
                        doc.command_stack.redo(doc.graph);
                        doc.dirty      = true;
                        doc.lint_dirty = true;
                        dirty_rebuild  = true;
                    }
                    break;
                }
                case canvas::Event::DoubleClicked:
                {
                    NodeId const id = NodeId(ev.node.v);
                    if (id != invalid_node_id)
                    {
                        // Label takes priority over Node since labels
                        // share the NodeId space.
                        if (doc.graph.find_label(id) != nullptr)
                        {
                            doc.popup.editing_label     = id;
                            doc.popup.label_name_buf_id = invalid_label_id;
                        }
                        else if (doc.graph.find_node(id) != nullptr)
                        {
                            doc.popup.editing_node     = id;
                            doc.popup.node_name_buf_id = invalid_node_id;
                        }
                    }
                    else
                    {
                        // Hit-test annotations on empty canvas.
                        Point const p{ ev.pos.x, ev.pos.y };
                        for (auto const& a : doc.graph.annotations())
                        {
                            if (p.x >= a.pos.x and p.x < a.pos.x + a.size.x
                                and p.y >= a.pos.y and p.y < a.pos.y + a.size.y)
                            {
                                doc.popup.editing_annotation = a.id;
                                break;
                            }
                        }
                    }
                    break;
                }
                case canvas::Event::ExtraDragStarted:
                {
                    Point const p{ ev.pos.x, ev.pos.y };
                    for (auto const& a : doc.graph.annotations())
                    {
                        if (p.x >= a.pos.x and p.x < a.pos.x + a.size.x
                            and p.y >= a.pos.y and p.y < a.pos.y + a.size.y)
                        {
                            doc.popup.dragging_annotation          = a.id;
                            doc.popup.annotation_drag_start_pos    = a.pos;
                            doc.popup.annotation_drag_start_canvas = ev.pos;
                            break;
                        }
                    }
                    break;
                }
                case canvas::Event::ExtraDragMoved:
                {
                    if (doc.popup.dragging_annotation != invalid_annotation_id)
                    {
                        Annotation* a =
                            doc.graph.find_annotation_mut(doc.popup.dragging_annotation);
                        if (a != nullptr)
                        {
                            a->pos.x = doc.popup.annotation_drag_start_pos.x
                                     + (ev.pos.x - doc.popup.annotation_drag_start_canvas.x);
                            a->pos.y = doc.popup.annotation_drag_start_pos.y
                                     + (ev.pos.y - doc.popup.annotation_drag_start_canvas.y);
                        }
                    }
                    break;
                }
                case canvas::Event::ExtraDragEnded:
                {
                    if (doc.popup.dragging_annotation != invalid_annotation_id)
                    {
                        Annotation* a =
                            doc.graph.find_annotation_mut(doc.popup.dragging_annotation);
                        if (a != nullptr)
                        {
                            Point const final_pos{
                                doc.popup.annotation_drag_start_pos.x
                                    + (ev.pos.x - doc.popup.annotation_drag_start_canvas.x),
                                doc.popup.annotation_drag_start_pos.y
                                    + (ev.pos.y - doc.popup.annotation_drag_start_canvas.y),
                            };
                            // Restore the original position on the live
                            // graph, then apply the command so apply()
                            // moves it forward and revert() snaps back.
                            a->pos = doc.popup.annotation_drag_start_pos;
                            if (final_pos != doc.popup.annotation_drag_start_pos)
                            {
                                doc.command_stack.push(std::make_unique<SetAnnotationPosCommand>(
                            doc.popup.dragging_annotation, final_pos), doc.graph);
                                doc.dirty = true;
                            }
                        }
                        doc.popup.dragging_annotation = invalid_annotation_id;
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
