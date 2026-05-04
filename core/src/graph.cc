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
        // Nodes and Labels share the NodeId space; reject collisions
        // with either kind so loaders can't produce ambiguous lookups.
        if (find_node(node.id) != nullptr or find_label(node.id) != nullptr)
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
        if (Label const* l = find_label(ref.node); l != nullptr)
        {
            return ref.attr == label_pin_name;
        }
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

    bool Graph::set_node_note(NodeId id, std::string const& note)
    {
        Node* n = find_node_mut(id);
        if (n == nullptr)
        {
            return false;
        }
        n->note = note;
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

    bool Graph::set_stage_color(std::string_view name, rgba color)
    {
        for (auto& s : stages_)
        {
            if (s.name == name)
            {
                s.color = color;
                return true;
            }
        }
        return false;
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

    void Graph::reserve_annotation_id_above(AnnotationId id)
    {
        if (id >= next_annotation_id_)
        {
            next_annotation_id_ = id + 1;
        }
    }

    AnnotationId Graph::add_annotation(Annotation const& a)
    {
        Annotation copy = a;
        copy.id = next_annotation_id_++;
        annotations_.push_back(copy);
        return copy.id;
    }

    bool Graph::insert_annotation(Annotation const& a)
    {
        if (a.id == invalid_annotation_id)
        {
            return false;
        }
        for (auto const& existing : annotations_)
        {
            if (existing.id == a.id) { return false; }
        }
        annotations_.push_back(a);
        if (a.id >= next_annotation_id_)
        {
            next_annotation_id_ = a.id + 1;
        }
        return true;
    }

    bool Graph::insert_annotation_at(Annotation const& a, std::size_t index)
    {
        if (a.id == invalid_annotation_id)
        {
            return false;
        }
        for (auto const& existing : annotations_)
        {
            if (existing.id == a.id) { return false; }
        }
        if (index > annotations_.size())
        {
            index = annotations_.size();
        }
        annotations_.insert(annotations_.begin() + index, a);
        if (a.id >= next_annotation_id_)
        {
            next_annotation_id_ = a.id + 1;
        }
        return true;
    }

    void Graph::remove_annotation(AnnotationId id)
    {
        annotations_.erase(
            std::remove_if(annotations_.begin(), annotations_.end(),
                           [id](Annotation const& a) { return a.id == id; }),
            annotations_.end());
    }

    bool Graph::set_annotation_pos(AnnotationId id, Point pos)
    {
        Annotation* a = find_annotation_mut(id);
        if (a == nullptr) { return false; }
        a->pos = pos;
        return true;
    }

    bool Graph::set_annotation_size(AnnotationId id, Point size)
    {
        Annotation* a = find_annotation_mut(id);
        if (a == nullptr) { return false; }
        a->size = size;
        return true;
    }

    bool Graph::set_annotation_text(AnnotationId id, std::string const& text)
    {
        Annotation* a = find_annotation_mut(id);
        if (a == nullptr) { return false; }
        a->text = text;
        return true;
    }

    bool Graph::set_annotation_color(AnnotationId id, rgba color)
    {
        Annotation* a = find_annotation_mut(id);
        if (a == nullptr) { return false; }
        a->color = color;
        return true;
    }

    Annotation const* Graph::find_annotation(AnnotationId id) const
    {
        for (auto const& a : annotations_)
        {
            if (a.id == id) { return &a; }
        }
        return nullptr;
    }

    Annotation* Graph::find_annotation_mut(AnnotationId id)
    {
        for (auto& a : annotations_)
        {
            if (a.id == id) { return &a; }
        }
        return nullptr;
    }

    LabelId Graph::add_label(LabelKind kind, std::string const& name, Point pos)
    {
        Label l;
        l.id   = next_node_id_++;
        l.kind = kind;
        l.name = name;
        l.pos  = pos;
        labels_.push_back(l);
        return l.id;
    }

    bool Graph::insert_label(Label const& l)
    {
        if (l.id == invalid_label_id) { return false; }
        for (auto const& existing : labels_)
        {
            if (existing.id == l.id) { return false; }
        }
        for (auto const& n : nodes_)
        {
            if (n.id == l.id) { return false; }
        }
        labels_.push_back(l);
        if (l.id >= next_node_id_)
        {
            next_node_id_ = l.id + 1;
        }
        return true;
    }

    bool Graph::insert_label_at(Label const& l, std::size_t index)
    {
        if (l.id == invalid_label_id) { return false; }
        for (auto const& existing : labels_)
        {
            if (existing.id == l.id) { return false; }
        }
        for (auto const& n : nodes_)
        {
            if (n.id == l.id) { return false; }
        }
        if (index > labels_.size()) { index = labels_.size(); }
        labels_.insert(labels_.begin() + index, l);
        if (l.id >= next_node_id_)
        {
            next_node_id_ = l.id + 1;
        }
        return true;
    }

    void Graph::remove_label(LabelId id)
    {
        labels_.erase(
            std::remove_if(labels_.begin(), labels_.end(),
                           [id](Label const& l) { return l.id == id; }),
            labels_.end());
        // Cascade incident links so the graph stays consistent.
        links_.erase(
            std::remove_if(links_.begin(), links_.end(),
                           [id](Link const& l)
                           {
                               return l.from.node == id or l.to.node == id;
                           }),
            links_.end());
    }

    bool Graph::set_label_name(LabelId id, std::string const& name)
    {
        Label* l = find_label_mut(id);
        if (l == nullptr) { return false; }
        l->name = name;
        return true;
    }

    bool Graph::set_label_pos(LabelId id, Point pos)
    {
        Label* l = find_label_mut(id);
        if (l == nullptr) { return false; }
        l->pos = pos;
        return true;
    }

    bool Graph::set_label_color(LabelId id, rgba color)
    {
        Label* l = find_label_mut(id);
        if (l == nullptr) { return false; }
        l->color = color;
        return true;
    }

    Label const* Graph::find_label(LabelId id) const
    {
        for (auto const& l : labels_)
        {
            if (l.id == id) { return &l; }
        }
        return nullptr;
    }

    Label* Graph::find_label_mut(LabelId id)
    {
        for (auto& l : labels_)
        {
            if (l.id == id) { return &l; }
        }
        return nullptr;
    }
}
