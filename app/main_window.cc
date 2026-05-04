#include "piper/app/main_window.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
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

#include "piper/app/bundled_fonts.h"
#include "piper/app/bundled_licenses.h"
#include "piper/app/project_license.h"
#include "piper/app/settings.h"
#include "piper/app/theme_loader.h"
#include "piper/builtin_nodes.h"
#include "piper/canvas/cull.h"
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
                doc.command_stack.push(
                    std::make_unique<MoveNodeCommand>(t.first, np), doc.graph);
            }
        }
        doc.command_stack.close_group();
        doc.dirty = true;
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
        float a = targets.front().second.y;
        float b = targets.back().second.y;
        if (horizontal)
        {
            a = targets.front().second.x;
            b = targets.back().second.x;
        }
        float const step = (b - a) / float(targets.size() - 1);

        doc.command_stack.open_group();
        for (std::size_t i = 0; i < targets.size(); ++i)
        {
            Point np = targets[i].second;
            float const target = a + step * float(i);
            if (horizontal) { np.x = target; }
            else            { np.y = target; }
            if (np != targets[i].second)
            {
                doc.command_stack.push(
                    std::make_unique<MoveNodeCommand>(targets[i].first, np), doc.graph);
            }
        }
        doc.command_stack.close_group();
        doc.dirty = true;
        doc.adapter.rebuild();
    }

    std::string autosave_dir()
    {
        char const* xdg = std::getenv("XDG_DATA_HOME");
        if (xdg != nullptr and *xdg != '\0')
        {
            return std::string(xdg) + "/piper/autosave";
        }
        char const* home = std::getenv("HOME");
        if (home != nullptr and *home != '\0')
        {
            return std::string(home) + "/.local/share/piper/autosave";
        }
        return std::string{};
    }

    void MainWindow::autosave_doc(Document& doc)
    {
        std::string const dir = autosave_dir();
        if (dir.empty())
        {
            return;
        }
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec)
        {
            return;
        }
        std::string const path = dir + "/session-" + std::to_string(doc.session_id) + ".piper";
        std::string const json = piper::v2::serialize(doc.graph, doc.pipeline_name);
        std::ofstream f(path);
        if (not f.is_open() or not (f << json))
        {
            return;
        }
        doc.autosave_path    = path;
        doc.last_autosave_at = std::chrono::steady_clock::now();
    }

    void MainWindow::clear_autosave(Document& doc)
    {
        if (doc.autosave_path.empty())
        {
            return;
        }
        std::error_code ec;
        std::filesystem::remove(doc.autosave_path, ec);
        doc.autosave_path.clear();
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
                autosave_doc(*doc);
            }
        }
    }

    void MainWindow::draw_minimap(Document& doc)
    {
        auto const nodes = doc.adapter.nodes();
        if (nodes.empty())
        {
            return;
        }

        canvas::Aabb overall = canvas::node_aabb(nodes[0], canvas_layout_);
        for (std::size_t i = 1; i < nodes.size(); ++i)
        {
            canvas::Aabb const a = canvas::node_aabb(nodes[i], canvas_layout_);
            overall.min.x = std::min(overall.min.x, a.min.x);
            overall.min.y = std::min(overall.min.y, a.min.y);
            overall.max.x = std::max(overall.max.x, a.max.x);
            overall.max.y = std::max(overall.max.y, a.max.y);
        }

        float const mm_w   = 200.0f * dpi_scale_;
        float const mm_h   = 150.0f * dpi_scale_;
        float const margin = 12.0f * dpi_scale_;
        ImVec2 const pane_origin = doc.editor.last_origin_screen();
        ImVec2 const pane_size   = doc.editor.last_size_screen();
        if (pane_size.x < mm_w + 2.0f * margin
            or pane_size.y < mm_h + 2.0f * margin)
        {
            return;
        }
        ImVec2 const mm_min{
            pane_origin.x + pane_size.x - mm_w - margin,
            pane_origin.y + pane_size.y - mm_h - margin,
        };
        ImVec2 const mm_max{ mm_min.x + mm_w, mm_min.y + mm_h };

        float const aabb_w = std::max(overall.max.x - overall.min.x, 1.0f);
        float const aabb_h = std::max(overall.max.y - overall.min.y, 1.0f);
        float const inset  = 4.0f;
        float const sx     = (mm_w - 2.0f * inset) / aabb_w;
        float const sy     = (mm_h - 2.0f * inset) / aabb_h;
        float const s      = std::min(sx, sy);
        float const ox     = mm_min.x + (mm_w - aabb_w * s) * 0.5f;
        float const oy     = mm_min.y + (mm_h - aabb_h * s) * 0.5f;

        auto canvas_to_mm = [&](ImVec2 const& cp)
        {
            return ImVec2{
                ox + (cp.x - overall.min.x) * s,
                oy + (cp.y - overall.min.y) * s,
            };
        };

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(mm_min, mm_max, IM_COL32(0x10, 0x10, 0x10, 0xC8), 4.0f);
        dl->AddRect(mm_min, mm_max, IM_COL32(0x55, 0x55, 0x55, 0xFF), 4.0f);
        for (auto const& n : nodes)
        {
            canvas::Aabb const a = canvas::node_aabb(n, canvas_layout_);
            ImVec2 const tl = canvas_to_mm(a.min);
            ImVec2 const br = canvas_to_mm(a.max);
            dl->AddRectFilled(tl, br, n.header_color);
        }

        float const zoom = doc.editor.zoom();
        if (zoom > 0.0f)
        {
            ImVec2 const pan = doc.editor.pan();
            ImVec2 const vp_min_canvas = pan;
            ImVec2 const vp_max_canvas{
                pan.x + pane_size.x / zoom,
                pan.y + pane_size.y / zoom,
            };
            ImVec2 vp_min = canvas_to_mm(vp_min_canvas);
            ImVec2 vp_max = canvas_to_mm(vp_max_canvas);
            // Clamp to mini-map rect so an off-graph viewport stays
            // visually inside the bezel.
            vp_min.x = std::clamp(vp_min.x, mm_min.x, mm_max.x);
            vp_min.y = std::clamp(vp_min.y, mm_min.y, mm_max.y);
            vp_max.x = std::clamp(vp_max.x, mm_min.x, mm_max.x);
            vp_max.y = std::clamp(vp_max.y, mm_min.y, mm_max.y);
            dl->AddRect(vp_min, vp_max, IM_COL32(0xFF, 0xC0, 0x40, 0xFF),
                        0.0f, 0, 1.5f);
        }

        bool const hovering = ImGui::IsMouseHoveringRect(mm_min, mm_max);
        if (hovering and ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            ImVec2 const m = ImGui::GetMousePos();
            ImVec2 const cp{
                overall.min.x + (m.x - ox) / s,
                overall.min.y + (m.y - oy) / s,
            };
            doc.editor.center_on(cp);
        }
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
        try_load_theme();

        Settings const s = load_settings();
        if (s.font_path.has_value())
        {
            theme_.font_path = *s.font_path;
        }
        if (s.font_size.has_value())
        {
            theme_.font_size = *s.font_size;
        }

        apply_current_theme();

        std::string const ad = autosave_dir();
        if (not ad.empty())
        {
            std::error_code ec;
            int found = 0;
            if (std::filesystem::is_directory(ad, ec))
            {
                for (auto const& entry : std::filesystem::directory_iterator(ad, ec))
                {
                    if (entry.is_regular_file() and entry.path().extension() == ".piper")
                    {
                        ++found;
                    }
                }
            }
            if (found > 0)
            {
                push_toast(ToastLevel::Warn,
                           std::to_string(found) + " autosave file(s) at "
                           + ad);
            }
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
        doc->adapter.rebuild();
        Document& ref = *doc;
        documents_.push_back(std::move(doc));
        active_doc_idx_ = int(documents_.size()) - 1;
        ++next_untitled_id_;
        return ref;
    }

    static ImU32 to_im_alpha(rgba c, float alpha_mul)
    {
        uint32_t r = c.r();
        uint32_t g = c.g();
        uint32_t b = c.b();
        uint32_t a = uint32_t(float(c.a()) * alpha_mul);
        if (a > 255u) { a = 255u; }
        return IM_COL32(r, g, b, a);
    }

    void MainWindow::wire_document_callbacks(Document& doc)
    {
        Document* dp = &doc;
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
                        if (ImGui::MenuItem("Delete annotation"))
                        {
                            dp->command_stack.push(
                                std::make_unique<DeleteAnnotationCommand>(hovered_anno),
                                dp->graph);
                            dp->dirty = true;
                        }
                        ImGui::EndPopup();
                        return;
                    }
                }

                if (ImGui::MenuItem("Add annotation here"))
                {
                    Annotation a;
                    a.pos  = Point{ canvas_pos.x, canvas_pos.y };
                    a.text = "Note";
                    dp->command_stack.push(
                        std::make_unique<AddAnnotationCommand>(a),
                        dp->graph);
                    dp->dirty = true;
                }

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
                    bool const unset_sel = node->stage.empty();
                    if (ImGui::MenuItem("(unset)", nullptr, unset_sel)
                        and not unset_sel)
                    {
                        dp->command_stack.push(
                            std::make_unique<SetNodeStageCommand>(
                                node->id, std::string{}),
                            dp->graph);
                        dp->dirty = true;
                        dp->adapter.rebuild();
                    }
                    for (auto const& s : dp->graph.stages())
                    {
                        bool const sel = (s.name == node->stage);
                        if (ImGui::MenuItem(s.name.c_str(), nullptr, sel)
                            and not sel)
                        {
                            dp->command_stack.push(
                                std::make_unique<SetNodeStageCommand>(
                                    node->id, s.name),
                                dp->graph);
                            dp->dirty = true;
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
            target->editor.request_fit();

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
                doc.command_stack.push(
                    std::make_unique<SetAttributeStagesCommand>(new_id, a.name, a.stages),
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

            for (auto const& a : n.attrs)
            {
                if (a.role != AttributeSpec::Role::Input)
                {
                    continue;
                }
                if (connected.count({ n.id, a.name }) == 0)
                {
                    Diagnostic d;
                    d.kind      = Diagnostic::Kind::SchemaError;
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
                d.kind    = Diagnostic::Kind::SchemaError;
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
        doc.command_stack.push(
            std::make_unique<AddNodeCommand>(type, name, doc.current_stage, pos),
            doc.graph);
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
        clear_autosave(doc);
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
        poll_autosave();
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
            if (ImGui::BeginMenu("Edit"))
            {
                Document* d = active();
                bool const have_sel =
                    d != nullptr and d->editor.selection_ids().size() >= 2;
                if (ImGui::BeginMenu("Align", have_sel))
                {
                    if (ImGui::MenuItem("Left"))
                    {
                        align_selection(*d, AlignMode::Left);
                    }
                    if (ImGui::MenuItem("Right"))
                    {
                        align_selection(*d, AlignMode::Right);
                    }
                    if (ImGui::MenuItem("Top"))
                    {
                        align_selection(*d, AlignMode::Top);
                    }
                    if (ImGui::MenuItem("Bottom"))
                    {
                        align_selection(*d, AlignMode::Bottom);
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Center horizontally"))
                    {
                        align_selection(*d, AlignMode::CenterH);
                    }
                    if (ImGui::MenuItem("Center vertically"))
                    {
                        align_selection(*d, AlignMode::CenterV);
                    }
                    ImGui::EndMenu();
                }
                bool const can_distribute =
                    d != nullptr and d->editor.selection_ids().size() >= 3;
                if (ImGui::BeginMenu("Distribute", can_distribute))
                {
                    if (ImGui::MenuItem("Horizontally"))
                    {
                        distribute_selection(*d, true);
                    }
                    if (ImGui::MenuItem("Vertically"))
                    {
                        distribute_selection(*d, false);
                    }
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
                if (ImGui::MenuItem("Mini-map", nullptr, minimap_visible_))
                {
                    minimap_visible_ = not minimap_visible_;
                }
                if (ImGui::MenuItem("Font..."))
                {
                    font_picker_open_ = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                if (ImGui::MenuItem("Shortcuts..."))
                {
                    shortcuts_open_ = true;
                }
                if (ImGui::MenuItem("About Piper..."))
                {
                    about_open_ = true;
                }
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
        if (minimap_visible_)
        {
            draw_minimap(adoc);
        }
        {
            canvas::NodeId const hovered = adoc.editor.hovered_node();
            if (hovered.v != 0)
            {
                Node const* hn = adoc.graph.find_node(NodeId(hovered.v));
                NodeType const* hnt = nullptr;
                if (hn != nullptr)
                {
                    hnt = registry_.find(hn->type);
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
        ImVec2 const mouse  = ImGui::GetMousePos();
        ImVec2 const origin = adoc.editor.last_origin_screen();
        ImVec2 const size   = adoc.editor.last_size_screen();
        bool const inside =
            size.x > 0.0f and size.y > 0.0f
            and mouse.x >= origin.x and mouse.x < origin.x + size.x
            and mouse.y >= origin.y and mouse.y < origin.y + size.y;
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
        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F,
                            ImGuiInputFlags_RouteAlways))
        {
            find_open_     = true;
            find_focus_    = true;
            find_buf_.fill('\0');
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
            std::strncpy(font_path_buf_.data(), theme_.font_path.c_str(),
                         font_path_buf_.size() - 1);
            font_path_buf_.back() = '\0';
            font_pending_size_    = theme_.font_size;
            font_filter_buf_.fill('\0');
        }
        if (ImGui::BeginPopup("##font_picker"))
        {
            ImGui::TextUnformatted("Font");
            ImGui::Separator();

            ImGui::SetNextItemWidth(360.0f);
            ImGui::InputTextWithHint("filter", "search...",
                                     font_filter_buf_.data(),
                                     font_filter_buf_.size());

            std::string const filter{font_filter_buf_.data()};
            ImVec2 const list_size{ 480.0f, 240.0f };
            if (ImGui::BeginListBox("##font_list", list_size))
            {
                for (auto const& bf : piper::app::bundled_fonts())
                {
                    std::string const path = std::string{"bundled:"} + bf.name;
                    if (not filter.empty()
                        and path.find(filter) == std::string::npos)
                    {
                        continue;
                    }
                    std::string const label = std::string{"(bundled) "} + bf.name;
                    bool const sel = (path == std::string{ font_path_buf_.data() });
                    ImGui::PushID(path.c_str());
                    if (ImGui::Selectable(label.c_str(), sel))
                    {
                        std::strncpy(font_path_buf_.data(), path.c_str(),
                                     font_path_buf_.size() - 1);
                        font_path_buf_.back() = '\0';
                    }
                    ImGui::PopID();
                }
                for (auto const& path : system_fonts_)
                {
                    if (not filter.empty()
                        and path.find(filter) == std::string::npos)
                    {
                        continue;
                    }
                    std::string const label = std::filesystem::path(path).filename().string();
                    bool const sel = (path == std::string{ font_path_buf_.data() });
                    ImGui::PushID(path.c_str());
                    if (ImGui::Selectable(label.c_str(), sel))
                    {
                        std::strncpy(font_path_buf_.data(), path.c_str(),
                                     font_path_buf_.size() - 1);
                        font_path_buf_.back() = '\0';
                    }
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s", path.c_str());
                    }
                    ImGui::PopID();
                }
                ImGui::EndListBox();
            }

            ImGui::SetNextItemWidth(360.0f);
            ImGui::InputText("path", font_path_buf_.data(), font_path_buf_.size());
            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                auto picked = pfd::open_file(
                    "Select font",
                    dialog_start_dir(font_path_buf_.data()),
                    { "Fonts", "*.ttf *.otf *.TTF *.OTF", "All files", "*" }).result();
                if (not picked.empty())
                {
                    std::strncpy(font_path_buf_.data(), picked.front().c_str(),
                                 font_path_buf_.size() - 1);
                    font_path_buf_.back() = '\0';
                }
            }
            ImGui::TextDisabled("Empty path = ImGui built-in");

            ImGui::SetNextItemWidth(120.0f);
            ImGui::DragFloat("size", &font_pending_size_, 0.5f, 8.0f, 48.0f, "%.1f px");

            ImGui::Separator();
            if (ImGui::Button("Apply"))
            {
                std::string const new_path{ font_path_buf_.data() };
                if (new_path != theme_.font_path or font_pending_size_ != theme_.font_size)
                {
                    theme_.font_path   = new_path;
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
            bool const enter = ImGui::InputText("##find_input",
                                                find_buf_.data(),
                                                find_buf_.size(),
                                                ImGuiInputTextFlags_EnterReturnsTrue);

            std::string const query{find_buf_.data()};
            std::vector<NodeId> matches;
            for (auto const& n : adoc.graph.nodes())
            {
                if (query.empty() or n.name.find(query) != std::string::npos)
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
            ImGui::TextUnformatted(piper::app::project_license());
            ImGui::EndChild();

            ImGui::Separator();
            ImGui::TextUnformatted("Bundled assets");

            auto licenses = piper::app::bundled_licenses();
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
                case canvas::EventKind::CutRequested:
                {
                    if (ev.selection.empty())
                    {
                        break;
                    }
                    copy_to_clipboard(doc, ev.selection);
                    doc.command_stack.open_group();
                    for (canvas::NodeId const cn : ev.selection)
                    {
                        doc.command_stack.push(
                            std::make_unique<DeleteNodeCommand>(NodeId(cn.v)),
                            doc.graph);
                    }
                    doc.command_stack.close_group();
                    doc.dirty     = true;
                    dirty_rebuild = true;
                    break;
                }
                case canvas::EventKind::DuplicateRequested:
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
