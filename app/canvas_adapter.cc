#include "piper/app/canvas_adapter.h"

#include <algorithm>
#include <functional>

#include <imgui.h>

#include "piper/app/theme_loader.h"
#include "piper/attribute.h"
#include "piper/node.h"
#include "piper/node_type.h"

namespace piper::app
{
    uint32_t type_tag_of(std::string const& data_type)
    {
        std::size_t const h = std::hash<std::string>{}(data_type);
        return uint32_t(h);
    }

    canvas::PinKind kind_of(AttributeSpec::Role r)
    {
        if (r == AttributeSpec::Role::Output)
        {
            return canvas::PinKind::Output;
        }
        return canvas::PinKind::Input;
    }

    PiperCanvasGraph::PiperCanvasGraph(piper::Graph const&        graph,
                                       piper::NodeRegistry const& registry,
                                       piper::Theme const&        theme)
        : graph_(graph)
        , registry_(registry)
        , theme_(theme)
    {
        rebuild();
    }

    void PiperCanvasGraph::set_current_stage(std::string const& stage_name)
    {
        current_stage_ = stage_name;
    }

    void PiperCanvasGraph::set_active_mode_profile(std::string const& name)
    {
        active_mode_profile_ = name;
    }

    namespace
    {
        // Multiplies RGB by `scale` (clamped to [0, 1]) while keeping
        // alpha. Used to make out-of-stage nodes read as muted without
        // going translucent (which would let the canvas bg bleed
        // through).
        ImU32 darken(ImU32 color, float scale)
        {
            if (scale < 0.0f)
            {
                scale = 0.0f;
            }
            if (scale > 1.0f)
            {
                scale = 1.0f;
            }
            uint32_t r = (color >> IM_COL32_R_SHIFT) & 0xFFu;
            uint32_t g = (color >> IM_COL32_G_SHIFT) & 0xFFu;
            uint32_t b = (color >> IM_COL32_B_SHIFT) & 0xFFu;
            uint32_t const a = (color >> IM_COL32_A_SHIFT) & 0xFFu;
            r = uint32_t(float(r) * scale);
            g = uint32_t(float(g) * scale);
            b = uint32_t(float(b) * scale);
            return IM_COL32(r, g, b, a);
        }

        bool attr_active_in_stage(piper::Node const&        node,
                                   piper::Attribute const&  attr,
                                   std::string const&       current)
        {
            if (current.empty())
            {
                return true;
            }
            if (attr.stages.empty())
            {
                return node.stage == current;
            }
            for (auto const& s : attr.stages)
            {
                if (s == current)
                {
                    return true;
                }
            }
            return false;
        }

        // A node is "running" in the current stage if its node-level
        // stage matches OR any of its per-pin overrides matches (the
        // Bus pattern: one node spans multiple stages).
        bool node_active_in_stage(piper::Node const& node,
                                  std::string const& current)
        {
            if (current.empty())
            {
                return true;
            }
            if (node.stage == current)
            {
                return true;
            }
            for (auto const& a : node.attrs)
            {
                if (a.role == AttributeSpec::Role::Member)
                {
                    continue;
                }
                for (auto const& s : a.stages)
                {
                    if (s == current)
                    {
                        return true;
                    }
                }
            }
            return false;
        }
    }

    canvas::Connect PiperCanvasGraph::can_connect(canvas::Pin const& a,
                                                  canvas::Pin const& b) const
    {
        if (a.type_tag != b.type_tag)
        {
            return canvas::Connect::TypeMismatch;
        }
        return canvas::Connect::Allow;
    }

    canvas::PinId PiperCanvasGraph::ref_to_pin_id(PinRef const& ref) const
    {
        auto const it = forward_.find(PinKey{ ref.node, ref.attr });
        if (it == forward_.end())
        {
            return canvas::invalid_pin_id;
        }
        return it->second;
    }

    PinRef PiperCanvasGraph::pin_id_to_ref(canvas::PinId id) const
    {
        auto const it = reverse_.find(id.v);
        if (it == reverse_.end())
        {
            return PinRef{};
        }
        return it->second;
    }

    void PiperCanvasGraph::rebuild()
    {
        mirror_nodes_.clear();
        mirror_links_.clear();
        inputs_.clear();
        outputs_.clear();
        forward_.clear();
        reverse_.clear();
        next_pin_id_ = 1;

        auto const& src_nodes = graph_.nodes();
        inputs_.resize(src_nodes.size());
        outputs_.resize(src_nodes.size());
        mirror_nodes_.reserve(src_nodes.size());

        ImU32 const default_header = to_imu32(theme_.node_default_header);
        ImU32 const default_body   = to_imu32(theme_.node_default_body);

        for (std::size_t i = 0; i < src_nodes.size(); ++i)
        {
            piper::Node const& n = src_nodes[i];

            for (auto const& a : n.attrs)
            {
                if (a.role == AttributeSpec::Role::Member)
                {
                    continue;
                }
                canvas::PinId const pid{ next_pin_id_++ };
                forward_[PinKey{ n.id, a.name }] = pid;
                reverse_[pid.v]                  = PinRef{ n.id, a.name };

                bool  const active   = attr_active_in_stage(n, a, current_stage_);

                rgba const  c        = color_for_type(theme_, a.data_type);
                ImU32       pin_rgb  = to_imu32(c);
                if (not active)
                {
                    pin_rgb = darken(pin_rgb, theme_.pin_alpha_inactive);
                }

                canvas::Pin pin{};
                pin.id       = pid;
                pin.kind     = kind_of(a.role);
                pin.label    = a.name;
                pin.color    = pin_rgb;
                pin.type_tag = type_tag_of(a.data_type);

                if (a.role == AttributeSpec::Role::Output)
                {
                    outputs_[i].push_back(pin);
                }
                else
                {
                    inputs_[i].push_back(pin);
                }
            }
        }

        // Look up the active mode profile (if any). Each node may
        // carry a label inside its per_node map; absence = "enable".
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

        // canvas::Node spans must point into the now-stable inputs_
        // and outputs_ inner vectors (vector-of-vector move keeps
        // inner heap pointers; we won't push more pins after this).
        for (std::size_t i = 0; i < src_nodes.size(); ++i)
        {
            piper::Node const& n = src_nodes[i];

            // Stage filter dims the node's body+header so the user
            // sees at a glance which nodes "run" in the current
            // stage. Links stay full-color.
            bool const  active   = node_active_in_stage(n, current_stage_);
            ImU32       header_c = default_header;
            ImU32       body_c   = default_body;
            float       body_a   = 1.0f;

            // Mode overlay: built-in labels "enable" / "disable" plus
            // custom labels resolved via theme.mode_colors. Stage
            // dimming is applied on top so an out-of-stage disabled
            // node reads as both faded AND darker.
            std::string mode_label;
            if (active_profile != nullptr)
            {
                auto const it = active_profile->per_node.find(n.id);
                if (it != active_profile->per_node.end())
                {
                    mode_label = it->second;
                }
            }
            if (mode_label == "disable")
            {
                body_a = theme_.node_body_alpha_disabled;
            }
            else if (not mode_label.empty() and mode_label != "enable")
            {
                auto const it = theme_.mode_colors.find(mode_label);
                if (it != theme_.mode_colors.end())
                {
                    body_c = to_imu32(it->second);
                }
                else
                {
                    // Unknown mode label: fallback magenta so the
                    // user notices the missing color-table entry.
                    body_c = IM_COL32(0xFF, 0x00, 0xFF, 0xFF);
                }
            }

            if (not active)
            {
                header_c = darken(header_c, 0.5f);
                body_c   = darken(body_c,   0.6f);
            }

            canvas::Node cn{};
            cn.id            = canvas::NodeId{ n.id };
            cn.title         = n.name;
            cn.pos           = ImVec2{ n.pos.x, n.pos.y };
            cn.header_color  = header_c;
            cn.body_color    = body_c;
            cn.body_alpha    = body_a;
            cn.body_min_size = ImVec2{ 0.0f, 0.0f };
            cn.inputs        = inputs_[i];
            cn.outputs       = outputs_[i];
            mirror_nodes_.push_back(cn);
        }

        // Links carry no stage information -- they are pure data flow
        // between nodes. Stage filtering only affects nodes (and the
        // pins inside them when per-pin overrides are set).
        ImU32 const default_link = IM_COL32(0xC0, 0xC0, 0xC0, 0xFF);
        for (auto const& l : graph_.links())
        {
            canvas::PinId const from = ref_to_pin_id(l.from);
            canvas::PinId const to   = ref_to_pin_id(l.to);
            if (from == canvas::invalid_pin_id or to == canvas::invalid_pin_id)
            {
                continue;
            }
            mirror_links_.push_back(canvas::Link{
                canvas::LinkId{ l.id },
                from,
                to,
                default_link,
            });
        }
    }
}
