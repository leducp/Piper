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

        NodeId const assigned = node.id;
        nodes_.push_back(node);
        return assigned;
    }

    bool Graph::insert_node(Node const& node)
    {
        if (find_node(node.id) != nullptr)
        {
            return false;
        }
        nodes_.push_back(node);
        if (node.id >= next_node_id_)
        {
            next_node_id_ = node.id + 1;
        }
        return true;
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
        return n->find_attr(ref.attr) != nullptr;
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

        LinkId const assigned = link.id;
        links_.push_back(link);
        return assigned;
    }

    bool Graph::insert_link(Link const& link)
    {
        if (find_link(link.id) != nullptr)
        {
            return false;
        }
        if (not resolve_pin(link.from) or not resolve_pin(link.to))
        {
            return false;
        }
        links_.push_back(link);
        if (link.id >= next_link_id_)
        {
            next_link_id_ = link.id + 1;
        }
        return true;
    }

    void Graph::remove_link(LinkId id)
    {
        auto pred = [id](Link const& l) { return l.id == id; };
        links_.erase(std::remove_if(links_.begin(), links_.end(), pred),
                     links_.end());
    }

    bool Graph::set_attr_value(NodeId id,
                               std::string_view attr_name,
                               std::string const& value)
    {
        Node* n = find_node_mut(id);
        if (n == nullptr)
        {
            return false;
        }
        for (auto& a : n->attrs)
        {
            if (a.name == attr_name)
            {
                a.value = value;
                return true;
            }
        }
        return false;
    }

    bool Graph::set_attr_stages(NodeId id,
                                std::string_view attr_name,
                                std::vector<std::string> const& stages)
    {
        Node* n = find_node_mut(id);
        if (n == nullptr)
        {
            return false;
        }
        for (auto& a : n->attrs)
        {
            if (a.name == attr_name)
            {
                a.stages = stages;
                return true;
            }
        }
        return false;
    }

    bool Graph::move_node(NodeId id, Point pos)
    {
        Node* n = find_node_mut(id);
        if (n == nullptr)
        {
            return false;
        }
        n->pos = pos;
        return true;
    }

    bool Graph::set_node_stage(NodeId id, std::string const& stage)
    {
        Node* n = find_node_mut(id);
        if (n == nullptr)
        {
            return false;
        }
        n->stage = stage;
        return true;
    }

    bool Graph::rename_node(NodeId id, std::string const& new_name)
    {
        Node* n = find_node_mut(id);
        if (n == nullptr)
        {
            return false;
        }
        n->name = new_name;
        return true;
    }

    bool Graph::add_stage(Stage const& stage)
    {
        for (auto const& s : stages_)
        {
            if (s.name == stage.name)
            {
                return false;
            }
        }
        stages_.push_back(stage);
        return true;
    }

    void Graph::remove_stage(std::string_view name)
    {
        auto pred = [name](Stage const& s) { return s.name == name; };
        stages_.erase(std::remove_if(stages_.begin(), stages_.end(), pred),
                      stages_.end());
    }

    bool Graph::move_stage_up(std::string_view name)
    {
        for (std::size_t i = 0; i < stages_.size(); ++i)
        {
            if (stages_[i].name != name)
            {
                continue;
            }
            if (i == 0)
            {
                return false;
            }
            std::swap(stages_[i], stages_[i - 1]);
            return true;
        }
        return false;
    }

    bool Graph::move_stage_down(std::string_view name)
    {
        for (std::size_t i = 0; i < stages_.size(); ++i)
        {
            if (stages_[i].name != name)
            {
                continue;
            }
            if (i + 1 >= stages_.size())
            {
                return false;
            }
            std::swap(stages_[i], stages_[i + 1]);
            return true;
        }
        return false;
    }

    bool Graph::insert_stage_at(Stage const& stage, std::size_t index)
    {
        for (auto const& s : stages_)
        {
            if (s.name == stage.name)
            {
                return false;
            }
        }
        if (index > stages_.size())
        {
            index = stages_.size();
        }
        stages_.insert(stages_.begin() + index, stage);
        return true;
    }

    void Graph::set_stages_order(std::vector<std::string> const& order)
    {
        std::vector<Stage> reordered;
        reordered.reserve(stages_.size());
        for (auto const& name : order)
        {
            auto it = std::find_if(stages_.begin(), stages_.end(),
                                   [name](Stage const& s) { return s.name == name; });
            if (it == stages_.end())
            {
                continue;
            }
            reordered.push_back(std::move(*it));
            stages_.erase(it);
        }
        for (auto& leftover : stages_)
        {
            reordered.push_back(std::move(leftover));
        }
        stages_ = std::move(reordered);
    }

    bool Graph::insert_mode_profile_at(ModeProfile const& profile, std::size_t index)
    {
        for (auto const& m : modes_)
        {
            if (m.name == profile.name)
            {
                return false;
            }
        }
        if (index > modes_.size())
        {
            index = modes_.size();
        }
        modes_.insert(modes_.begin() + index, profile);
        return true;
    }

    bool Graph::move_stage_to(std::string_view name, std::string_view target)
    {
        if (name == target)
        {
            return false;
        }

        // Capture indices BEFORE the erase so the insertion point is
        // computed in original-coordinate space. Inserting at
        // tgt_idx in the post-erase vector lands the moved stage:
        //   - after target  when src was above target (src < tgt)
        //   - before target when src was below target (src > tgt)
        // matching typical drag-drop semantics.
        int src_idx = -1;
        int tgt_idx = -1;
        for (std::size_t i = 0; i < stages_.size(); ++i)
        {
            if (stages_[i].name == name)
            {
                src_idx = int(i);
            }
            if (not target.empty() and stages_[i].name == target)
            {
                tgt_idx = int(i);
            }
        }
        if (src_idx < 0)
        {
            return false;
        }

        Stage moved = std::move(stages_[src_idx]);
        stages_.erase(stages_.begin() + src_idx);

        if (tgt_idx < 0)
        {
            stages_.push_back(std::move(moved));
            return true;
        }
        stages_.insert(stages_.begin() + tgt_idx, std::move(moved));
        return true;
    }

    bool Graph::add_mode_profile(ModeProfile const& profile)
    {
        for (auto const& m : modes_)
        {
            if (m.name == profile.name)
            {
                return false;
            }
        }
        modes_.push_back(profile);
        return true;
    }

    void Graph::remove_mode_profile(std::string_view name)
    {
        auto pred = [name](ModeProfile const& m) { return m.name == name; };
        modes_.erase(std::remove_if(modes_.begin(), modes_.end(), pred),
                     modes_.end());
    }

    bool Graph::set_node_mode_label(std::string_view  profile,
                                     NodeId            node_id,
                                     std::string const& label)
    {
        for (auto& m : modes_)
        {
            if (m.name != profile)
            {
                continue;
            }
            if (label.empty())
            {
                m.per_node.erase(node_id);
            }
            else
            {
                m.per_node[node_id] = label;
            }
            return true;
        }
        return false;
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

    void Graph::reserve_ids_above(NodeId max_node_id, LinkId max_link_id)
    {
        if (max_node_id >= next_node_id_)
        {
            next_node_id_ = max_node_id + 1;
        }
        if (max_link_id >= next_link_id_)
        {
            next_link_id_ = max_link_id + 1;
        }
    }
}
