#include "piper/app/canvas_adapter.h"

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

                rgba const  c     = color_for_type(theme_, a.data_type);
                canvas::Pin pin{};
                pin.id       = pid;
                pin.kind     = kind_of(a.role);
                pin.label    = a.name;
                pin.color    = to_imu32(c);
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

        // canvas::Node spans must point into the now-stable inputs_
        // and outputs_ inner vectors (vector-of-vector move keeps
        // inner heap pointers; we won't push more pins after this).
        for (std::size_t i = 0; i < src_nodes.size(); ++i)
        {
            piper::Node const& n = src_nodes[i];

            canvas::Node cn{};
            cn.id            = canvas::NodeId{ n.id };
            cn.title         = n.name;
            cn.pos           = ImVec2{ n.pos.x, n.pos.y };
            cn.header_color  = default_header;
            cn.body_color    = default_body;
            cn.body_alpha    = 1.0f;
            cn.body_min_size = ImVec2{ 0.0f, 0.0f };
            cn.inputs        = inputs_[i];
            cn.outputs       = outputs_[i];
            mirror_nodes_.push_back(cn);
        }

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
