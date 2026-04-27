#include "piper/graph.h"

#include <algorithm>

namespace piper
{
    NodeId Graph::add_node(NodeType const& type,
                           std::string const& name,
                           std::string const& stage,
                           Point pos)
    {
        Node node;
        node.id    = next_node_id_++;
        node.type  = type.type;
        node.name  = name;
        node.stage = stage;
        node.pos   = pos;

        node.attrs.reserve(type.attributes.size());
        for (auto const& spec : type.attributes)
        {
            Attribute attr;
            attr.name      = spec.name;
            attr.data_type = spec.data_type;
            attr.role      = spec.role;
            attr.value     = spec.default_value;
            node.attrs.push_back(attr);
        }

        nodes_.push_back(node);
        return nodes_.back().id;
    }

    void Graph::remove_node(NodeId id)
    {
        auto link_pred = [id](Link const& l)
        {
            return l.from.node == id or l.to.node == id;
        };
        links_.erase(std::remove_if(links_.begin(), links_.end(), link_pred),
                     links_.end());

        auto node_pred = [id](Node const& n) { return n.id == id; };
        nodes_.erase(std::remove_if(nodes_.begin(), nodes_.end(), node_pred),
                     nodes_.end());
    }

    bool Graph::resolve_pin(PinRef const& ref) const
    {
        Node const* n = find_node(ref.node);
        if (n == nullptr)
        {
            return false;
        }
        for (auto const& a : n->attrs)
        {
            if (a.name == ref.attr)
            {
                return true;
            }
        }
        return false;
    }

    LinkId Graph::add_link(PinRef const& from, PinRef const& to, std::string const& data_type)
    {
        if (not resolve_pin(from) or not resolve_pin(to))
        {
            return invalid_link_id;
        }

        Link link;
        link.id        = next_link_id_++;
        link.from      = from;
        link.to        = to;
        link.data_type = data_type;

        links_.push_back(link);
        return links_.back().id;
    }

    void Graph::remove_link(LinkId id)
    {
        auto pred = [id](Link const& l) { return l.id == id; };
        links_.erase(std::remove_if(links_.begin(), links_.end(), pred),
                     links_.end());
    }

    void Graph::add_stage(Stage const& stage)
    {
        stages_.push_back(stage);
    }

    void Graph::remove_stage(std::string_view name)
    {
        auto pred = [name](Stage const& s) { return s.name == name; };
        stages_.erase(std::remove_if(stages_.begin(), stages_.end(), pred),
                      stages_.end());

        for (auto& n : nodes_)
        {
            if (n.stage == name)
            {
                n.stage.clear();
            }
        }
    }

    void Graph::add_mode_profile(ModeProfile const& profile)
    {
        modes_.push_back(profile);
    }

    void Graph::remove_mode_profile(std::string_view name)
    {
        auto pred = [name](ModeProfile const& m) { return m.name == name; };
        modes_.erase(std::remove_if(modes_.begin(), modes_.end(), pred),
                     modes_.end());
    }

    Node const* Graph::find_node(NodeId id) const
    {
        for (auto const& n : nodes_)
        {
            if (n.id == id)
            {
                return &n;
            }
        }
        return nullptr;
    }

    Link const* Graph::find_link(LinkId id) const
    {
        for (auto const& l : links_)
        {
            if (l.id == id)
            {
                return &l;
            }
        }
        return nullptr;
    }

    Node* Graph::find_node_mut(NodeId id)
    {
        for (auto& n : nodes_)
        {
            if (n.id == id)
            {
                return &n;
            }
        }
        return nullptr;
    }
}
