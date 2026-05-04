#include "piper/canvas/editor.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "piper/canvas/aabb.h"
#include "piper/canvas/cull.h"
#include "piper/canvas/graph.h"
#include "piper/canvas/hit_test.h"

namespace piper::canvas
{
    constexpr float zoom_min  = 0.1f;
    constexpr float zoom_max  = 10.0f;
    constexpr float zoom_step = 1.1f;

    ImU32 apply_alpha(ImU32 color, float alpha)
    {
        float const clamped = std::clamp(alpha, 0.0f, 1.0f);
        uint32_t const original_a = (color >> IM_COL32_A_SHIFT) & 0xFFu;
        uint32_t const scaled_a   = uint32_t(float(original_a) * clamped);
        return (color & ~uint32_t(IM_COL32_A_MASK))
             | (scaled_a << IM_COL32_A_SHIFT);
    }

    void draw_label_pentagon(ImDrawList* draw_list,
                              Node const& node,
                              Style const& style,
                              ImVec2 const& top_left,
                              ImVec2 const& bot_right,
                              float zoom,
                              bool selected)
    {
        // Pentagon: rectangle on the pin side, triangular nub on the
        // abstraction side. Pin side faces the canvas (where the wire
        // attaches); abstraction side carries the chevron tip.
        float const tip_extent =
            (bot_right.y - top_left.y) * 0.5f;  // half-height-deep nub
        float const mid_y = (top_left.y + bot_right.y) * 0.5f;

        ImVec2 pts[5];
        if (node.shape == Shape::LabelIn)
        {
            // pin on left flat edge, tip protrudes to the right
            float const flat_right = bot_right.x - tip_extent;
            pts[0] = ImVec2{ top_left.x,  top_left.y  };
            pts[1] = ImVec2{ flat_right,  top_left.y  };
            pts[2] = ImVec2{ bot_right.x, mid_y       };
            pts[3] = ImVec2{ flat_right,  bot_right.y };
            pts[4] = ImVec2{ top_left.x,  bot_right.y };
        }
        else
        {
            // tip on left, pin on right flat edge
            float const flat_left = top_left.x + tip_extent;
            pts[0] = ImVec2{ flat_left,   top_left.y  };
            pts[1] = ImVec2{ bot_right.x, top_left.y  };
            pts[2] = ImVec2{ bot_right.x, bot_right.y };
            pts[3] = ImVec2{ flat_left,   bot_right.y };
            pts[4] = ImVec2{ top_left.x,  mid_y       };
        }

        ImU32 const fill = node.header_color;
        draw_list->AddConvexPolyFilled(pts, 5, fill);

        ImU32 outline_color     = style.node_outline;
        float outline_thickness = 1.0f;
        if (selected)
        {
            outline_color     = style.node_outline_selected;
            outline_thickness = 2.0f;
        }
        draw_list->AddPolyline(pts, 5, outline_color,
                               ImDrawFlags_Closed, outline_thickness);

        if (not node.title.empty())
        {
            ImFont* const font      = ImGui::GetFont();
            float   const font_size = ImGui::GetFontSize() * zoom;
            // Shift the title past the chevron tip on LabelOut so it
            // doesn't bleed into the triangular cut-out.
            float text_left = top_left.x + style.node_padding.x * zoom;
            if (node.shape == Shape::LabelOut)
            {
                text_left += tip_extent;
            }
            ImVec2 const title_pos{ text_left,
                                     top_left.y + style.node_padding.y * zoom };
            draw_list->AddText(font, font_size, title_pos, IM_COL32_WHITE,
                               node.title.data(),
                               node.title.data() + node.title.size());
        }
    }

    void draw_node_body(ImDrawList* draw_list,
                        Node const& node,
                        Style const& style,
                        LayoutMetrics const& metrics,
                        Transform const& transform,
                        ImVec2 const& origin,
                        ImVec2 const& canvas_offset,
                        bool selected)
    {
        Aabb local = node_aabb(node, metrics);
        local.min.x += canvas_offset.x;
        local.min.y += canvas_offset.y;
        local.max.x += canvas_offset.x;
        local.max.y += canvas_offset.y;
        ImVec2 const top_left  = transform.to_screen(local.min, origin);
        ImVec2 const bot_right = transform.to_screen(local.max, origin);

        if (node.shape != Shape::Rect)
        {
            draw_label_pentagon(draw_list, node, style,
                                top_left, bot_right, transform.zoom, selected);
            return;
        }

        float const header_h_screen = metrics.header_height * transform.zoom;
        ImVec2 const header_br{ bot_right.x, top_left.y + header_h_screen };

        ImU32 const body_color = apply_alpha(node.body_color, node.body_alpha);

        draw_list->AddRectFilled(top_left, bot_right, body_color, style.node_rounding);
        if (node.header_bands.empty())
        {
            draw_list->AddRectFilled(top_left, header_br, node.header_color,
                                     style.node_rounding, ImDrawFlags_RoundCornersTop);
        }
        else
        {
            std::size_t const n_bands = node.header_bands.size();
            float const band_w = (header_br.x - top_left.x) / float(n_bands);
            for (std::size_t b = 0; b < n_bands; ++b)
            {
                float const x0 = top_left.x + band_w * float(b);
                float       x1 = x0 + band_w;
                if (b + 1 == n_bands)
                {
                    x1 = header_br.x;   // avoid sub-pixel gap at the edge
                }
                ImDrawFlags flags = ImDrawFlags_None;
                if (b == 0)
                {
                    flags |= ImDrawFlags_RoundCornersTopLeft;
                }
                if (b + 1 == n_bands)
                {
                    flags |= ImDrawFlags_RoundCornersTopRight;
                }
                draw_list->AddRectFilled(ImVec2{ x0, top_left.y },
                                         ImVec2{ x1, header_br.y },
                                         node.header_bands[b],
                                         style.node_rounding, flags);
            }
        }

        std::size_t const pin_rows  = std::max(node.inputs.size(), node.outputs.size());
        float const       row_h     = metrics.pin_row_height * transform.zoom;
        ImU32 const       row_alt   = apply_alpha(style.node_row_alt, node.body_alpha);
        for (std::size_t r = 0; r < pin_rows; ++r)
        {
            if (r % 2 != 1)
            {
                continue;
            }
            float const  y_top = header_br.y + float(r) * row_h;
            float        y_bot = y_top + row_h;
            if (y_bot > bot_right.y)
            {
                y_bot = bot_right.y;
            }
            draw_list->AddRectFilled(ImVec2{ top_left.x, y_top },
                                     ImVec2{ bot_right.x, y_bot }, row_alt);
        }

        // Separator between pin rows and the host's body content area.
        // Only meaningful when both regions exist.
        if (pin_rows > 0 and node.body_min_size.y > 0.0f)
        {
            float const y_sep = header_br.y + float(pin_rows) * row_h;
            draw_list->AddLine(ImVec2{ top_left.x,  y_sep },
                               ImVec2{ bot_right.x, y_sep },
                               style.node_separator);
        }

        ImU32 outline_color     = style.node_outline;
        float outline_thickness = 1.0f;
        if (selected)
        {
            outline_color     = style.node_outline_selected;
            outline_thickness = 2.0f;
        }
        draw_list->AddRect(top_left, bot_right, outline_color,
                           style.node_rounding, 0, outline_thickness);

        if (not node.title.empty())
        {
            ImFont* const font      = ImGui::GetFont();
            float   const font_size = ImGui::GetFontSize() * transform.zoom;
            ImVec2  const title_pos{
                top_left.x + style.node_padding.x * transform.zoom,
                top_left.y + style.node_padding.y * transform.zoom,
            };
            draw_list->AddText(font, font_size, title_pos, IM_COL32_WHITE,
                               node.title.data(),
                               node.title.data() + node.title.size());
        }
    }

    Aabb make_aabb(ImVec2 const& a, ImVec2 const& b)
    {
        return Aabb{
            ImVec2{ std::min(a.x, b.x), std::min(a.y, b.y) },
            ImVec2{ std::max(a.x, b.x), std::max(a.y, b.y) },
        };
    }

    void draw_pin(ImDrawList* draw_list,
                  Pin const& pin,
                  PinKind kind,
                  ImVec2 const& center_screen,
                  Style const& style,
                  float zoom)
    {
        float const radius = style.pin_radius * zoom;
        draw_list->AddCircleFilled(center_screen, radius, pin.color);
        draw_list->AddCircle(center_screen, radius, style.node_outline);

        if (pin.label.empty())
        {
            return;
        }

        ImFont* const font      = ImGui::GetFont();
        float   const font_size = ImGui::GetFontSize() * zoom;
        ImVec2  const text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f,
                                                      pin.label.data(),
                                                      pin.label.data() + pin.label.size());

        float const gap = 4.0f * zoom;
        ImVec2 label_pos;
        if (kind == PinKind::Input)
        {
            label_pos = ImVec2{ center_screen.x + radius + gap,
                                center_screen.y - text_size.y * 0.5f };
        }
        else
        {
            label_pos = ImVec2{ center_screen.x - radius - gap - text_size.x,
                                center_screen.y - text_size.y * 0.5f };
        }
        draw_list->AddText(font, font_size, label_pos, IM_COL32_WHITE,
                           pin.label.data(),
                           pin.label.data() + pin.label.size());
    }

    Editor::Editor(Graph& source)
        : source_(source)
    {
    }

    float Editor::pin_hit_radius() const
    {
        // Use whichever is bigger in canvas units: 2 x the visible
        // pin radius, or 12 screen px translated back through the
        // current zoom. The screen-px floor keeps the hit area large
        // enough to click comfortably even at 0.3x zoom.
        float const r_canvas = style_.pin_radius * 2.0f;
        if (transform_.zoom <= 0.0f)
        {
            return r_canvas;
        }
        float const r_floor = 12.0f / transform_.zoom;
        if (r_floor > r_canvas)
        {
            return r_floor;
        }
        return r_canvas;
    }

    LinkId Editor::hit_test_link_at(ImVec2 mouse_screen, ImVec2 const& origin,
                                     float tolerance) const
    {
        LinkId best = invalid_link_id;
        float best_d2 = tolerance * tolerance;
        constexpr int n_samples = 24;
        for (auto const& link : source_.links())
        {
            auto from_it = pin_index_.find(link.from);
            auto to_it   = pin_index_.find(link.to);
            if (from_it == pin_index_.end() or to_it == pin_index_.end())
            {
                continue;
            }
            ImVec2 const a = transform_.to_screen(from_it->second.center, origin);
            ImVec2 const b = transform_.to_screen(to_it->second.center,   origin);
            BezierPoints const bez = link_bezier(
                a, b, style_.link_bezier_strength * transform_.zoom);
            for (int i = 0; i <= n_samples; ++i)
            {
                float const t  = float(i) / float(n_samples);
                float const u  = 1.0f - t;
                float const w0 = u * u * u;
                float const w1 = 3.0f * u * u * t;
                float const w2 = 3.0f * u * t * t;
                float const w3 = t * t * t;
                ImVec2 const p{
                    w0 * bez.a.x + w1 * bez.c1.x + w2 * bez.c2.x + w3 * bez.b.x,
                    w0 * bez.a.y + w1 * bez.c1.y + w2 * bez.c2.y + w3 * bez.b.y,
                };
                float const dx = p.x - mouse_screen.x;
                float const dy = p.y - mouse_screen.y;
                float const d2 = dx * dx + dy * dy;
                if (d2 < best_d2)
                {
                    best_d2 = d2;
                    best    = link.id;
                }
            }
        }
        return best;
    }

    void Editor::draw(ImVec2 const& size)
    {
        ImVec2 const origin{ImGui::GetCursorScreenPos()};
        last_origin_ = origin;
        last_size_   = size;

        if (pending_fit_)
        {
            pending_fit_ = false;
            zoom_to_fit(pending_fit_ids_);
            pending_fit_ids_.clear();
        }
        ImVec2 const br{ origin.x + size.x, origin.y + size.y };

        auto const&    nodes         = source_.nodes();
        ImGuiIO const& io            = ImGui::GetIO();
        ImVec2 const   cursor_canvas = transform_.to_canvas(io.MousePos, origin);

        // ----- Render pass -----
        // Uses state set by the previous frame's input pass; widgets
        // submitted in body_renderer below are submitted *before* the
        // canvas InvisibleButton, so they win clicks. Drag/connect
        // visuals lag input by one frame (~17ms at 60fps).

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(origin, br, style_.canvas_bg);
        draw_list->PushClipRect(origin, br, true);

        ImVec2 const canvas_min = transform_.to_canvas(origin, origin);
        ImVec2 const canvas_max = transform_.to_canvas(br, origin);
        float  const spacing    = style_.grid_spacing;
        for (float x = std::floor(canvas_min.x / spacing) * spacing;
             x < canvas_max.x;
             x += spacing)
        {
            ImVec2 const a = transform_.to_screen({ x, canvas_min.y }, origin);
            ImVec2 const b = transform_.to_screen({ x, canvas_max.y }, origin);
            draw_list->AddLine(a, b, style_.grid_line);
        }
        for (float y = std::floor(canvas_min.y / spacing) * spacing;
             y < canvas_max.y;
             y += spacing)
        {
            ImVec2 const a = transform_.to_screen({ canvas_min.x, y }, origin);
            ImVec2 const b = transform_.to_screen({ canvas_max.x, y }, origin);
            draw_list->AddLine(a, b, style_.grid_line);
        }

        if (background_renderer_)
        {
            background_renderer_(draw_list, origin, size,
                                 transform_.zoom, transform_.pan);
        }

        pin_index_.clear();
        pin_index_.reserve(nodes.size() * 2);
        for (auto const& n : nodes)
        {
            ImVec2 offset{ 0.0f, 0.0f };
            if (dragging_nodes_ and selection_.contains(n.id))
            {
                offset = drag_delta_;
            }
            for (std::size_t i = 0; i < n.inputs.size(); ++i)
            {
                ImVec2 c = pin_center_in_node(n, PinKind::Input, i, layout_);
                c.x += offset.x;
                c.y += offset.y;
                pin_index_[n.inputs[i].id] = PinLocation{
                    n.id, PinKind::Input, i, c, &n.inputs[i],
                };
            }
            for (std::size_t i = 0; i < n.outputs.size(); ++i)
            {
                ImVec2 c = pin_center_in_node(n, PinKind::Output, i, layout_);
                c.x += offset.x;
                c.y += offset.y;
                pin_index_[n.outputs[i].id] = PinLocation{
                    n.id, PinKind::Output, i, c, &n.outputs[i],
                };
            }
        }

        float const link_thickness = style_.link_thickness * transform_.zoom;
        for (auto const& link : source_.links())
        {
            auto const& from_it = pin_index_.find(link.from);
            auto const& to_it   = pin_index_.find(link.to);
            if (from_it == pin_index_.end() or to_it == pin_index_.end())
            {
                continue;
            }
            ImVec2 const a = transform_.to_screen(from_it->second.center, origin);
            ImVec2 const b = transform_.to_screen(to_it->second.center,   origin);
            BezierPoints const bez = link_bezier(a, b, style_.link_bezier_strength * transform_.zoom);
            ImU32 col       = link.color;
            float thickness = link_thickness;
            if (link.id == selected_link_)
            {
                col       = style_.node_outline_selected;
                thickness = link_thickness * 2.0f;
            }
            draw_list->AddBezierCubic(bez.a, bez.c1, bez.c2, bez.b, col, thickness);
        }

        Aabb const viewport{ canvas_min, canvas_max };
        auto const visible = cull_visible(nodes, viewport, layout_);
        for (auto idx : visible)
        {
            auto const& node     = nodes[idx];
            bool const  selected = selection_.contains(node.id);
            ImVec2      offset{ 0.0f, 0.0f };
            if (dragging_nodes_ and selected)
            {
                offset = drag_delta_;
            }
            draw_node_body(draw_list, node, style_, layout_, transform_, origin, offset, selected);

            if (body_renderer_)
            {
                Aabb local = node_aabb(node, layout_);
                local.min.x += offset.x;
                local.min.y += offset.y;
                local.max.x += offset.x;
                local.max.y += offset.y;
                ImVec2 const      tl       = transform_.to_screen(local.min, origin);
                ImVec2 const      br_node  = transform_.to_screen(local.max, origin);
                std::size_t const pin_rows = std::max(node.inputs.size(), node.outputs.size());
                float  const      header_h = layout_.header_height * transform_.zoom;
                float  const      pins_h   = float(pin_rows) * layout_.pin_row_height * transform_.zoom;
                // Skip the pin-row band so host content never overlaps
                // pin labels.
                ImVec2 const      body_min{ tl.x, tl.y + header_h + pins_h };
                body_renderer_(node.id, draw_list, body_min, br_node, transform_.zoom);
            }

            for (std::size_t i = 0; i < node.inputs.size(); ++i)
            {
                ImVec2 c_canvas = pin_center_in_node(node, PinKind::Input, i, layout_);
                c_canvas.x += offset.x;
                c_canvas.y += offset.y;
                ImVec2 const c_screen = transform_.to_screen(c_canvas, origin);
                draw_pin(draw_list, node.inputs[i], PinKind::Input, c_screen,
                         style_, transform_.zoom);
            }
            for (std::size_t i = 0; i < node.outputs.size(); ++i)
            {
                ImVec2 c_canvas = pin_center_in_node(node, PinKind::Output, i, layout_);
                c_canvas.x += offset.x;
                c_canvas.y += offset.y;
                ImVec2 const c_screen = transform_.to_screen(c_canvas, origin);
                draw_pin(draw_list, node.outputs[i], PinKind::Output, c_screen,
                         style_, transform_.zoom);
            }
        }

        if (connecting_)
        {
            auto const src_it = pin_index_.find(connect_from_pin_id_);
            if (src_it != pin_index_.end())
            {
                ImVec2 const src_canvas = src_it->second.center;
                Pin const&   src_pin    = *src_it->second.pin;

                float const r_hit       = pin_hit_radius();
                auto const  target      = hit_test_pin(nodes, cursor_canvas, layout_, r_hit);

                ImVec2  end_canvas = cursor_canvas;
                Connect status     = Connect::Allow;
                if (target.has_value() and target->pin->id != connect_from_pin_id_)
                {
                    end_canvas = target->center;
                    if (target->node_id == connect_from_node_id_)
                    {
                        status = Connect::SameNode;
                    }
                    else if (target->kind == connect_from_kind_)
                    {
                        status = Connect::KindMismatch;
                    }
                    else
                    {
                        Pin const& tgt_pin = *target->pin;
                        if (connect_from_kind_ == PinKind::Output)
                        {
                            status = source_.can_connect(src_pin, tgt_pin);
                        }
                        else
                        {
                            status = source_.can_connect(tgt_pin, src_pin);
                        }
                    }
                }

                ImVec2 a_canvas = src_canvas;
                ImVec2 b_canvas = end_canvas;
                if (connect_from_kind_ == PinKind::Input)
                {
                    a_canvas = end_canvas;
                    b_canvas = src_canvas;
                }
                ImVec2 const       a_screen = transform_.to_screen(a_canvas, origin);
                ImVec2 const       b_screen = transform_.to_screen(b_canvas, origin);
                BezierPoints const bez      = link_bezier(a_screen, b_screen,
                                                          style_.link_bezier_strength * transform_.zoom);
                ImU32 ghost_color = style_.link_invalid;
                if (status == Connect::Allow)
                {
                    ghost_color = style_.link_default;
                }
                draw_list->AddBezierCubic(bez.a, bez.c1, bez.c2, bez.b,
                                          ghost_color, link_thickness);
            }
        }

        if (box_selecting_)
        {
            Aabb const   box  = make_aabb(box_start_canvas_, box_current_canvas_);
            ImVec2 const tl   = transform_.to_screen(box.min, origin);
            ImVec2 const br_s = transform_.to_screen(box.max, origin);
            draw_list->AddRectFilled(tl, br_s, style_.selection_box);
            draw_list->AddRect(tl, br_s, style_.node_outline_selected);
        }

        draw_list->PopClipRect();

        // ----- Input pass -----
        // InvisibleButton submitted last so widgets above win the
        // active id when they overlap. State updated here is consumed
        // by the next frame's render.

        ImGui::SetCursorScreenPos(origin);
        ImGui::InvisibleButton("##canvas", size,
            ImGuiButtonFlags_MouseButtonLeft   |
            ImGuiButtonFlags_MouseButtonRight  |
            ImGuiButtonFlags_MouseButtonMiddle);
        bool const hovered = ImGui::IsItemHovered();

        bool selection_changed = false;

        if (hovered and ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
        {
            ImVec2 const delta = io.MouseDelta;
            transform_.pan.x -= delta.x / transform_.zoom;
            transform_.pan.y -= delta.y / transform_.zoom;
        }

        float const wheel = io.MouseWheel;
        if (hovered and wheel != 0.0f)
        {
            ImVec2 const canvas_before = transform_.to_canvas(io.MousePos, origin);
            float  const factor        = std::pow(zoom_step, wheel);
            transform_.zoom = std::clamp(transform_.zoom * factor, zoom_min, zoom_max);
            ImVec2 const canvas_after  = transform_.to_canvas(io.MousePos, origin);
            transform_.pan.x += canvas_before.x - canvas_after.x;
            transform_.pan.y += canvas_before.y - canvas_after.y;
        }

        last_hovered_node_ = NodeId{};
        if (hovered)
        {
            auto const hit = hit_test_node(nodes, cursor_canvas, layout_);
            if (hit.has_value())
            {
                last_hovered_node_ = *hit;
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)
                or ImGui::IsKeyPressed(ImGuiKey_Backspace, false))
            {
                if (selected_link_ != invalid_link_id)
                {
                    EventPayload ev{};
                    ev.kind = Event::LinkDeleted;
                    ev.link = selected_link_;
                    pending_events_.push_back(ev);
                    selected_link_ = invalid_link_id;
                }
                else if (not selection_.empty())
                {
                    for (NodeId const id : selection_.ids())
                    {
                        EventPayload ev{};
                        ev.kind = Event::NodeDeleted;
                        ev.node = id;
                        pending_events_.push_back(ev);
                    }
                    if (selection_.clear())
                    {
                        selection_changed = true;
                    }
                }
            }
            if (io.KeyCtrl and ImGui::IsKeyPressed(ImGuiKey_C, false))
            {
                EventPayload ev{};
                ev.kind      = Event::CopyRequested;
                {
                    auto const ids = selection_.ids();
                    ev.selection.assign(ids.begin(), ids.end());
                }
                pending_events_.push_back(ev);
            }
            if (io.KeyCtrl and ImGui::IsKeyPressed(ImGuiKey_V, false))
            {
                EventPayload ev{};
                ev.kind = Event::PasteRequested;
                ev.pos  = cursor_canvas;
                pending_events_.push_back(ev);
            }
            if (io.KeyCtrl and ImGui::IsKeyPressed(ImGuiKey_X, false))
            {
                EventPayload ev{};
                ev.kind      = Event::CutRequested;
                {
                    auto const ids = selection_.ids();
                    ev.selection.assign(ids.begin(), ids.end());
                }
                pending_events_.push_back(ev);
            }
            if (io.KeyCtrl and ImGui::IsKeyPressed(ImGuiKey_D, false))
            {
                EventPayload ev{};
                ev.kind      = Event::DuplicateRequested;
                {
                    auto const ids = selection_.ids();
                    ev.selection.assign(ids.begin(), ids.end());
                }
                ev.pos       = cursor_canvas;
                pending_events_.push_back(ev);
            }
            if (not io.KeyCtrl and ImGui::IsKeyPressed(ImGuiKey_F, false))
            {
                zoom_to_fit(selection_.ids());
            }
            if (io.KeyCtrl and ImGui::IsKeyPressed(ImGuiKey_Z, false))
            {
                EventPayload ev{};
                if (io.KeyShift)
                {
                    ev.kind = Event::RedoRequested;
                }
                else
                {
                    ev.kind = Event::UndoRequested;
                }
                pending_events_.push_back(ev);
            }
            if (io.KeyCtrl and ImGui::IsKeyPressed(ImGuiKey_Y, false))
            {
                EventPayload ev{};
                ev.kind = Event::RedoRequested;
                pending_events_.push_back(ev);
            }
        }

        if (hovered and ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            auto const hit       = hit_test_node(nodes, cursor_canvas, layout_);
            context_menu_node_   = hit.value_or(invalid_node_id);
            context_menu_canvas_ = cursor_canvas;

            EventPayload ev{};
            ev.kind = Event::ContextMenuRequested;
            ev.node = context_menu_node_;
            ev.pos  = cursor_canvas;
            pending_events_.push_back(ev);

            ImGui::OpenPopup("##canvas_ctx");
        }

        // Double-click: surface to host (covers nodes, labels, and
        // empty-canvas hits — host inspects the id and may hit-test
        // its own extras like annotations). ImGui fires both
        // IsMouseClicked and IsMouseDoubleClicked on the second click
        // of a pair, so we shortcut the regular click path below.
        bool const left_double_clicked =
            hovered and ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        if (left_double_clicked)
        {
            auto const hit = hit_test_node(nodes, cursor_canvas, layout_);
            EventPayload ev{};
            ev.kind = Event::DoubleClicked;
            ev.node = hit.value_or(invalid_node_id);
            ev.pos  = cursor_canvas;
            pending_events_.push_back(ev);
        }

        // Mouse-down dispatch: pin > node > empty.
        if (hovered and not left_double_clicked
            and ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            bool const shift   = ImGui::GetIO().KeyShift;
            float const r_hit  = pin_hit_radius();
            auto const  pin_at = hit_test_pin(nodes, cursor_canvas, layout_, r_hit);

            if (pin_at.has_value())
            {
                connecting_               = true;
                connect_from_pin_id_      = pin_at->pin->id;
                connect_from_kind_        = pin_at->kind;
                connect_from_node_id_     = pin_at->node_id;
                pending_reduce_to_single_ = false;
                selected_link_            = invalid_link_id;
            }
            else
            {
                auto const node_hit = hit_test_node(nodes, cursor_canvas, layout_);
                if (node_hit.has_value())
                {
                    selected_link_ = invalid_link_id;
                    if (shift)
                    {
                        if (selection_.toggle(*node_hit))
                        {
                            selection_changed = true;
                        }
                    }
                    else if (not selection_.contains(*node_hit))
                    {
                        NodeId const single[1] = { *node_hit };
                        if (selection_.set(single))
                        {
                            selection_changed = true;
                        }
                    }
                    else
                    {
                        pending_reduce_to_single_ = true;
                        pending_reduce_node_      = *node_hit;
                    }

                    drag_lead_start_pos_ = ImVec2{ 0.0f, 0.0f };
                    bool lead_found      = false;
                    for (auto const& n : nodes)
                    {
                        if (n.id == *node_hit)
                        {
                            drag_lead_start_pos_ = n.pos;
                            lead_found           = true;
                            break;
                        }
                    }
                    // hit_test_node just returned this id, so missing
                    // it from `nodes` means the host's mirror is out
                    // of sync. Skip the drag setup rather than anchor
                    // the snap at the canvas origin.
                    if (lead_found)
                    {
                    dragging_nodes_     = true;
                    drag_start_canvas_  = cursor_canvas;
                    drag_delta_         = ImVec2{ 0.0f, 0.0f };
                    drag_start_positions_.clear();
                    drag_start_positions_.reserve(selection_.size());
                    for (NodeId const id : selection_.ids())
                    {
                        for (auto const& n : nodes)
                        {
                            if (n.id == id)
                            {
                                drag_start_positions_.emplace_back(id, n.pos);
                                break;
                            }
                        }
                    }
                    }
                }
                else
                {
                    LinkId const link_hit = hit_test_link_at(io.MousePos, origin, 8.0f);
                    if (link_hit != invalid_link_id)
                    {
                        selected_link_ = link_hit;
                        if (selection_.clear())
                        {
                            selection_changed = true;
                        }
                    }
                    else if (extra_hit_test_ and extra_hit_test_(cursor_canvas))
                    {
                        // Host owns this drag (e.g. annotation). Suppress
                        // box-select for the duration; emit lifecycle
                        // events the host translates into entity moves.
                        selected_link_            = invalid_link_id;
                        extra_dragging_           = true;
                        extra_drag_last_canvas_   = cursor_canvas;
                        pending_reduce_to_single_ = false;
                        if (not shift and selection_.clear())
                        {
                            selection_changed = true;
                        }
                        EventPayload ev{};
                        ev.kind = Event::ExtraDragStarted;
                        ev.pos  = cursor_canvas;
                        pending_events_.push_back(ev);
                    }
                    else
                    {
                        selected_link_            = invalid_link_id;
                        box_selecting_            = true;
                        box_select_additive_      = shift;
                        box_start_canvas_         = cursor_canvas;
                        box_current_canvas_       = cursor_canvas;
                        pending_reduce_to_single_ = false;
                        box_select_base_.assign(selection_.ids().begin(), selection_.ids().end());
                        if (not shift and selection_.clear())
                        {
                            selection_changed = true;
                        }
                    }
                }
            }
        }

        if (extra_dragging_)
        {
            if (cursor_canvas.x != extra_drag_last_canvas_.x
                or cursor_canvas.y != extra_drag_last_canvas_.y)
            {
                extra_drag_last_canvas_ = cursor_canvas;
                EventPayload ev{};
                ev.kind = Event::ExtraDragMoved;
                ev.pos  = cursor_canvas;
                pending_events_.push_back(ev);
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                extra_dragging_ = false;
                EventPayload ev{};
                ev.kind = Event::ExtraDragEnded;
                ev.pos  = cursor_canvas;
                pending_events_.push_back(ev);
            }
        }

        if (dragging_nodes_)
        {
            ImVec2 const raw_delta{
                cursor_canvas.x - drag_start_canvas_.x,
                cursor_canvas.y - drag_start_canvas_.y,
            };
            drag_delta_ = raw_delta;

            // Snap the lead node's final position to the absolute
            // grid (round((lead_start + raw_delta) / g)). The other
            // selected nodes inherit the same delta so the cluster
            // shifts as one and the lead lands on grid regardless of
            // its starting offset.
            if (style_.snap_to_grid and style_.grid_spacing > 0.0f)
            {
                float const g = style_.grid_spacing;
                ImVec2 const naive_final{
                    drag_lead_start_pos_.x + raw_delta.x,
                    drag_lead_start_pos_.y + raw_delta.y,
                };
                ImVec2 const snapped_final{
                    std::round(naive_final.x / g) * g,
                    std::round(naive_final.y / g) * g,
                };
                drag_delta_ = ImVec2{
                    snapped_final.x - drag_lead_start_pos_.x,
                    snapped_final.y - drag_lead_start_pos_.y,
                };
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                float const screen_dx     = drag_delta_.x * transform_.zoom;
                float const screen_dy     = drag_delta_.y * transform_.zoom;
                bool  const moved_enough  = screen_dx * screen_dx + screen_dy * screen_dy > 16.0f;

                if (moved_enough)
                {
                    for (auto const& entry : drag_start_positions_)
                    {
                        EventPayload ev{};
                        ev.kind = Event::NodeMoved;
                        ev.node = entry.first;
                        ev.pos  = ImVec2{ entry.second.x + drag_delta_.x,
                                          entry.second.y + drag_delta_.y };
                        pending_events_.push_back(ev);
                    }
                }
                else if (pending_reduce_to_single_)
                {
                    NodeId const single[1] = { pending_reduce_node_ };
                    if (selection_.set(single))
                    {
                        selection_changed = true;
                    }
                }

                dragging_nodes_           = false;
                pending_reduce_to_single_ = false;
                drag_start_positions_.clear();
                drag_delta_               = ImVec2{ 0.0f, 0.0f };
            }
        }

        if (box_selecting_)
        {
            box_current_canvas_ = cursor_canvas;
            Aabb const  box     = make_aabb(box_start_canvas_, box_current_canvas_);
            auto const  hits    = nodes_in_box(nodes, box, layout_);

            std::vector<NodeId> next;
            next.reserve(box_select_base_.size() + hits.size());
            if (box_select_additive_)
            {
                next = box_select_base_;
            }
            for (auto idx : hits)
            {
                NodeId const id = nodes[idx].id;
                if (std::find(next.begin(), next.end(), id) == next.end())
                {
                    next.push_back(id);
                }
            }
            if (selection_.set(next))
            {
                selection_changed = true;
            }

            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                box_selecting_ = false;
                box_select_base_.clear();
            }
        }

        // Connect-release: emits LinkCreated when the user drops the
        // ghost link on a compatible pin. Re-evaluates can_connect at
        // release time to match the user-visible ghost color.
        if (connecting_ and ImGui::IsMouseReleased(ImGuiMouseButton_Left))
        {
            auto const src_it = pin_index_.find(connect_from_pin_id_);
            if (src_it != pin_index_.end())
            {
                Pin const& src_pin = *src_it->second.pin;
                float const r_hit  = pin_hit_radius();
                auto const  target = hit_test_pin(nodes, cursor_canvas, layout_, r_hit);

                if (target.has_value()
                    and target->pin->id  != connect_from_pin_id_
                    and target->node_id  != connect_from_node_id_
                    and target->kind     != connect_from_kind_)
                {
                    Pin const& tgt_pin = *target->pin;
                    Connect status = Connect::Allow;
                    if (connect_from_kind_ == PinKind::Output)
                    {
                        status = source_.can_connect(src_pin, tgt_pin);
                    }
                    else
                    {
                        status = source_.can_connect(tgt_pin, src_pin);
                    }
                    if (status == Connect::Allow)
                    {
                        EventPayload ev{};
                        ev.kind = Event::LinkCreated;
                        if (connect_from_kind_ == PinKind::Output)
                        {
                            ev.pin_from = connect_from_pin_id_;
                            ev.pin_to   = target->pin->id;
                        }
                        else
                        {
                            ev.pin_from = target->pin->id;
                            ev.pin_to   = connect_from_pin_id_;
                        }
                        pending_events_.push_back(ev);
                    }
                }
            }
            connecting_ = false;
        }

        if (context_menu_ and ImGui::BeginPopup("##canvas_ctx"))
        {
            context_menu_(context_menu_node_, context_menu_canvas_);
            ImGui::EndPopup();
        }

        if (selection_changed)
        {
            EventPayload ev{};
            ev.kind      = Event::SelectionChanged;
            {
                    auto const ids = selection_.ids();
                    ev.selection.assign(ids.begin(), ids.end());
                }
            pending_events_.push_back(ev);
        }
    }

    std::span<EventPayload const> Editor::consume_events()
    {
        drained_events_.clear();
        pending_events_.swap(drained_events_);
        return drained_events_;
    }

    void Editor::request_fit(std::span<NodeId const> ids)
    {
        pending_fit_ = true;
        pending_fit_ids_.assign(ids.begin(), ids.end());
    }

    void Editor::zoom_to_fit(std::span<NodeId const> ids)
    {
        if (last_size_.x <= 0.0f or last_size_.y <= 0.0f)
        {
            return;
        }
        auto const& nodes = source_.nodes();
        if (nodes.empty())
        {
            return;
        }

        bool have_aabb = false;
        Aabb merged{};
        for (auto const& n : nodes)
        {
            bool fit_this = ids.empty();
            if (not fit_this)
            {
                for (NodeId const id : ids)
                {
                    if (id == n.id) { fit_this = true; break; }
                }
            }
            if (not fit_this) { continue; }
            Aabb const a = node_aabb(n, layout_);
            if (not have_aabb)
            {
                merged    = a;
                have_aabb = true;
            }
            else
            {
                merged.min.x = std::min(merged.min.x, a.min.x);
                merged.min.y = std::min(merged.min.y, a.min.y);
                merged.max.x = std::max(merged.max.x, a.max.x);
                merged.max.y = std::max(merged.max.y, a.max.y);
            }
        }
        if (not have_aabb)
        {
            return;
        }

        float const w = std::max(merged.max.x - merged.min.x, 1.0f);
        float const h = std::max(merged.max.y - merged.min.y, 1.0f);
        constexpr float margin = 0.10f;
        float const target_w = last_size_.x * (1.0f - 2.0f * margin);
        float const target_h = last_size_.y * (1.0f - 2.0f * margin);
        float zoom = std::min(target_w / w, target_h / h);
        zoom       = std::clamp(zoom, zoom_min, zoom_max);
        transform_.zoom = zoom;
        ImVec2 const center{
            (merged.min.x + merged.max.x) * 0.5f,
            (merged.min.y + merged.max.y) * 0.5f,
        };
        transform_.pan.x = center.x - (last_size_.x * 0.5f) / zoom;
        transform_.pan.y = center.y - (last_size_.y * 0.5f) / zoom;
    }

    void Editor::center_on(ImVec2 const& canvas_pos)
    {
        if (transform_.zoom <= 0.0f or last_size_.x <= 0.0f or last_size_.y <= 0.0f)
        {
            return;
        }
        transform_.pan.x = canvas_pos.x - (last_size_.x * 0.5f) / transform_.zoom;
        transform_.pan.y = canvas_pos.y - (last_size_.y * 0.5f) / transform_.zoom;
    }

    void Editor::center_on(NodeId id)
    {
        if (transform_.zoom <= 0.0f or last_size_.x <= 0.0f or last_size_.y <= 0.0f)
        {
            return;
        }
        for (auto const& n : source_.nodes())
        {
            if (n.id != id)
            {
                continue;
            }
            Aabb const   aabb = node_aabb(n, layout_);
            ImVec2 const node_center{
                (aabb.min.x + aabb.max.x) * 0.5f,
                (aabb.min.y + aabb.max.y) * 0.5f,
            };
            // Pan is the canvas-space top-left of the viewport, so
            // putting `node_center` at the screen-space middle is:
            //   pan = node_center - viewport_size_canvas / 2.
            transform_.pan.x = node_center.x - (last_size_.x * 0.5f) / transform_.zoom;
            transform_.pan.y = node_center.y - (last_size_.y * 0.5f) / transform_.zoom;
            return;
        }
    }

    void Editor::scroll_to(NodeId id)
    {
        // Bring `id` into view if it is currently off-screen; leave
        // the pan alone otherwise.
        if (transform_.zoom <= 0.0f or last_size_.x <= 0.0f or last_size_.y <= 0.0f)
        {
            return;
        }
        for (auto const& n : source_.nodes())
        {
            if (n.id != id)
            {
                continue;
            }
            Aabb const aabb = node_aabb(n, layout_);
            // Current viewport in canvas space.
            float const v_min_x = transform_.pan.x;
            float const v_min_y = transform_.pan.y;
            float const v_max_x = v_min_x + last_size_.x / transform_.zoom;
            float const v_max_y = v_min_y + last_size_.y / transform_.zoom;
            bool const onscreen = aabb.min.x >= v_min_x and aabb.max.x <= v_max_x
                              and aabb.min.y >= v_min_y and aabb.max.y <= v_max_y;
            if (not onscreen)
            {
                center_on(id);
            }
            return;
        }
    }

    void Editor::set_selection(std::span<NodeId const> ids)
    {
        if (selection_.set(ids))
        {
            EventPayload ev{};
            ev.kind      = Event::SelectionChanged;
            {
                    auto const ids = selection_.ids();
                    ev.selection.assign(ids.begin(), ids.end());
                }
            pending_events_.push_back(ev);
        }
    }

    ImVec2 Editor::screen_to_canvas(ImVec2 const& screen) const
    {
        return transform_.to_canvas(screen, last_origin_);
    }

    ImVec2 Editor::canvas_to_screen(ImVec2 const& canvas) const
    {
        return transform_.to_screen(canvas, last_origin_);
    }
}
