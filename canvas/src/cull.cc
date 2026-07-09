#include <algorithm>
#include <cmath>

#include <imgui.h>

#include "piper/canvas/cull.h"

namespace piper::canvas
{
    namespace
    {
        // Width of a pin label at the unscaled (zoom 1) font, or 0
        // when the label is empty or no ImGui context is active.
        // Bare ImGui::CalcTextSize works fine at any frame stage as
        // long as a context exists; tests run without one and skip.
        float label_width(std::string_view label)
        {
            if (label.empty() or ImGui::GetCurrentContext() == nullptr)
            {
                return 0.0f;
            }
            return ImGui::CalcTextSize(label.data(),
                                       label.data() + label.size()).x;
        }
    }

    bool pin_on_left(PinKind kind, bool flip_side)
    {
        bool left = (kind == PinKind::Input);
        if (flip_side)
        {
            left = not left;
        }
        return left;
    }

    // Count of pins rendered on the given side (left = true).
    std::size_t pins_on_side(Node const& node, bool left)
    {
        std::size_t n = 0;
        for (auto const& p : node.inputs)
        {
            if (pin_on_left(PinKind::Input, p.flip_side) == left) { ++n; }
        }
        for (auto const& p : node.outputs)
        {
            if (pin_on_left(PinKind::Output, p.flip_side) == left) { ++n; }
        }
        return n;
    }

    // n-th pin on the given side, in encounter order (inputs then
    // outputs). nullptr when the column has fewer than n+1 pins.
    Pin const* nth_pin_on_side(Node const& node, bool left, std::size_t n)
    {
        std::size_t seen = 0;
        for (auto const& p : node.inputs)
        {
            if (pin_on_left(PinKind::Input, p.flip_side) == left)
            {
                if (seen == n) { return &p; }
                ++seen;
            }
        }
        for (auto const& p : node.outputs)
        {
            if (pin_on_left(PinKind::Output, p.flip_side) == left)
            {
                if (seen == n) { return &p; }
                ++seen;
            }
        }
        return nullptr;
    }

    float node_total_width(Node const& node, LayoutMetrics const& metrics)
    {
        float w = std::max(node.body_min_size.x, metrics.min_width);

        // Title must fit in the header; padding matches style.node_padding.x.
        constexpr float title_pad_x = 8.0f;
        float const     title_w     = label_width(node.title);
        if (title_w > 0.0f)
        {
            float const needed = title_w + 2.0f * title_pad_x;
            if (needed > w)
            {
                w = needed;
            }
        }

        // Auto-fit pin labels: each row may carry both an input
        // label (left-anchored) and an output label (right-
        // anchored), so the body must be wide enough that they do
        // not overlap.
        std::size_t const rows = std::max(pins_on_side(node, true),
                                          pins_on_side(node, false));
        for (std::size_t r = 0; r < rows; ++r)
        {
            float in_w  = 0.0f;
            float out_w = 0.0f;
            if (Pin const* lp = nth_pin_on_side(node, true, r); lp != nullptr)
            {
                in_w = label_width(lp->label);
            }
            if (Pin const* rp = nth_pin_on_side(node, false, r); rp != nullptr)
            {
                out_w = label_width(rp->label);
            }
            float const needed = in_w + out_w + metrics.label_padding;
            if (needed > w)
            {
                w = needed;
            }
        }
        return w;
    }

    Aabb node_aabb(Node const& node, LayoutMetrics const& metrics)
    {
        if (node.shape != Shape::Rect)
        {
            // Compact label pentagon: title text + padding, plus the
            // chevron-tip extent on the abstraction side. Padding
            // matches style.node_padding so the title sits flush.
            constexpr float pad_x = 8.0f;
            constexpr float pad_y = 6.0f;
            ImVec2 title_size{ 0.0f, 0.0f };
            if (not node.title.empty() and ImGui::GetCurrentContext() != nullptr)
            {
                title_size = ImGui::CalcTextSize(node.title.data(),
                                                  node.title.data() + node.title.size());
            }
            float const min_dim = metrics.pin_row_height;
            float const body_h  = std::max(title_size.y + 2.0f * pad_y, min_dim);
            float const tip     = body_h * 0.5f;
            float const body_w  = std::max(title_size.x + 2.0f * pad_x, min_dim) + tip;
            return Aabb{
                node.pos,
                ImVec2{ node.pos.x + body_w, node.pos.y + body_h },
            };
        }

        // Body height = pin rows + host-declared extra content,
        // floored by min_body_height so a node with no pins and no
        // extra content still has a clickable body.
        std::size_t const pin_rows      = std::max(pins_on_side(node, true),
                                                   pins_on_side(node, false));
        float const       pin_content_h = float(pin_rows) * metrics.pin_row_height;
        float const       extra         = std::max(0.0f, node.body_min_size.y);
        float const       content_h     = std::max(pin_content_h + extra, metrics.min_body_height);

        float const total_h = metrics.header_height + content_h;
        float const total_w = node_total_width(node, metrics);

        return Aabb{
            node.pos,
            ImVec2{ node.pos.x + total_w, node.pos.y + total_h },
        };
    }

    std::vector<std::size_t> cull_visible(
        std::span<Node const>  nodes,
        Aabb const&            viewport,
        LayoutMetrics const&   metrics)
    {
        std::vector<std::size_t> result;
        result.reserve(nodes.size());
        for (std::size_t i = 0; i < nodes.size(); ++i)
        {
            if (node_aabb(nodes[i], metrics).intersects(viewport))
            {
                result.push_back(i);
            }
        }
        return result;
    }

    ImVec2 pin_center_in_node(Node const& node,
                              PinKind kind,
                              std::size_t index,
                              LayoutMetrics const& metrics)
    {
        if (node.shape != Shape::Rect)
        {
            // Label pentagons hang their single pin on the flat edge
            // (opposite the chevron tip), centered on body mid-height.
            Aabb const a     = node_aabb(node, metrics);
            float const mid_y = (a.min.y + a.max.y) * 0.5f;
            if (node.shape == Shape::LabelIn)
            {
                return ImVec2{ a.min.x, mid_y };
            }
            return ImVec2{ a.max.x, mid_y };
        }
        bool flip = false;
        if (kind == PinKind::Input)
        {
            flip = node.inputs[index].flip_side;
        }
        else
        {
            flip = node.outputs[index].flip_side;
        }
        bool const left = pin_on_left(kind, flip);

        // Row within the pin's side-column: count same-side pins that
        // precede it in encounter order (inputs then outputs).
        std::size_t row   = 0;
        bool        found = false;
        for (std::size_t i = 0; i < node.inputs.size(); ++i)
        {
            if (kind == PinKind::Input and i == index) { found = true; break; }
            if (pin_on_left(PinKind::Input, node.inputs[i].flip_side) == left) { ++row; }
        }
        if (not found)
        {
            for (std::size_t i = 0; i < node.outputs.size(); ++i)
            {
                if (kind == PinKind::Output and i == index) { break; }
                if (pin_on_left(PinKind::Output, node.outputs[i].flip_side) == left) { ++row; }
            }
        }

        float const y = node.pos.y
                      + metrics.header_height
                      + (float(row) + 0.5f) * metrics.pin_row_height;
        if (left)
        {
            return ImVec2{ node.pos.x, y };
        }
        return ImVec2{ node.pos.x + node_total_width(node, metrics), y };
    }

    BezierPoints link_bezier(ImVec2 const& a, ImVec2 const& b, float strength)
    {
        float const ext = std::max(std::abs(b.x - a.x) * 0.5f, strength);
        return BezierPoints{
            a,
            ImVec2{ a.x + ext, a.y },
            ImVec2{ b.x - ext, b.y },
            b,
        };
    }
}
