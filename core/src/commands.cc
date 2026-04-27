#include "piper/commands.h"

#include "piper/graph.h"

namespace piper
{
    AddNodeCommand::AddNodeCommand(NodeType const& type,
                                   std::string const& name,
                                   std::string const& stage,
                                   Point pos)
        : type_(type)
    {
        snapshot_.name  = name;
        snapshot_.stage = stage;
        snapshot_.pos   = pos;
    }

    void AddNodeCommand::apply(Graph& g)
    {
        if (first_apply_)
        {
            first_apply_ = false;
            snapshot_.id = g.add_node(type_, snapshot_.name, snapshot_.stage, snapshot_.pos);
            if (snapshot_.id == invalid_node_id)
            {
                return;
            }
            // Capture synthesized attrs so redo can re-insert exactly.
            Node const* live = g.find_node(snapshot_.id);
            if (live != nullptr)
            {
                snapshot_ = *live;
            }
            return;
        }
        if (snapshot_.id != invalid_node_id)
        {
            g.insert_node(snapshot_);
        }
    }

    void AddNodeCommand::revert(Graph& g)
    {
        if (snapshot_.id == invalid_node_id)
        {
            return;
        }
        // Refresh in case other commands edited attrs since apply, so
        // redo restores the latest state.
        Node const* live = g.find_node(snapshot_.id);
        if (live != nullptr)
        {
            snapshot_ = *live;
        }
        g.remove_node(snapshot_.id);
    }

    void DeleteNodeCommand::apply(Graph& g)
    {
        if (not snapshot_.has_value())
        {
            Node const* live = g.find_node(id_);
            if (live != nullptr)
            {
                snapshot_ = *live;
            }
            // Snapshot incident links — Graph::remove_node cascades.
            for (auto const& l : g.links())
            {
                if (l.from.node == id_ or l.to.node == id_)
                {
                    incident_links_.push_back(l);
                }
            }
        }
        g.remove_node(id_);
    }

    void DeleteNodeCommand::revert(Graph& g)
    {
        if (not snapshot_.has_value())
        {
            return;
        }
        g.insert_node(*snapshot_);
        for (auto const& l : incident_links_)
        {
            g.insert_link(l);
        }
        snapshot_.reset();
        incident_links_.clear();
    }

    void MoveNodeCommand::apply(Graph& g)
    {
        if (not old_pos_.has_value())
        {
            Node const* live = g.find_node(id_);
            if (live != nullptr)
            {
                old_pos_ = live->pos;
            }
        }
        g.move_node(id_, new_pos_);
    }

    void MoveNodeCommand::revert(Graph& g)
    {
        if (old_pos_.has_value())
        {
            g.move_node(id_, *old_pos_);
            old_pos_.reset();
        }
    }

    bool MoveNodeCommand::try_merge(Command const& next)
    {
        auto const* other = dynamic_cast<MoveNodeCommand const*>(&next);
        if (other == nullptr or other->id_ != id_)
        {
            return false;
        }
        new_pos_ = other->new_pos_;
        return true;
    }

    void RenameNodeCommand::apply(Graph& g)
    {
        if (not old_name_.has_value())
        {
            Node const* live = g.find_node(id_);
            if (live != nullptr)
            {
                old_name_ = live->name;
            }
        }
        g.rename_node(id_, new_name_);
    }

    void RenameNodeCommand::revert(Graph& g)
    {
        if (old_name_.has_value())
        {
            g.rename_node(id_, *old_name_);
            old_name_.reset();
        }
    }

    void SetNodeStageCommand::apply(Graph& g)
    {
        if (not old_stage_.has_value())
        {
            Node const* live = g.find_node(id_);
            if (live != nullptr)
            {
                old_stage_ = live->stage;
            }
        }
        g.set_node_stage(id_, new_stage_);
    }

    void SetNodeStageCommand::revert(Graph& g)
    {
        if (old_stage_.has_value())
        {
            g.set_node_stage(id_, *old_stage_);
            old_stage_.reset();
        }
    }

    CreateLinkCommand::CreateLinkCommand(PinRef const& from,
                                         PinRef const& to,
                                         std::string const& data_type)
        : from_(from), to_(to), data_type_(data_type)
    {
    }

    void CreateLinkCommand::apply(Graph& g)
    {
        if (first_apply_)
        {
            first_apply_ = false;
            snapshot_.id = g.add_link(from_, to_, data_type_);
            if (snapshot_.id == invalid_link_id)
            {
                return;
            }
            Link const* live = g.find_link(snapshot_.id);
            if (live != nullptr)
            {
                snapshot_ = *live;
            }
            return;
        }
        if (snapshot_.id != invalid_link_id)
        {
            g.insert_link(snapshot_);
        }
    }

    void CreateLinkCommand::revert(Graph& g)
    {
        if (snapshot_.id != invalid_link_id)
        {
            g.remove_link(snapshot_.id);
        }
    }

    void DeleteLinkCommand::apply(Graph& g)
    {
        if (not snapshot_.has_value())
        {
            Link const* live = g.find_link(id_);
            if (live != nullptr)
            {
                snapshot_ = *live;
            }
        }
        g.remove_link(id_);
    }

    void DeleteLinkCommand::revert(Graph& g)
    {
        if (snapshot_.has_value())
        {
            g.insert_link(*snapshot_);
            snapshot_.reset();
        }
    }

    void SetAttributeValueCommand::apply(Graph& g)
    {
        if (not old_value_.has_value())
        {
            Node const* live = g.find_node(id_);
            if (live != nullptr)
            {
                Attribute const* a = live->find_attr(attr_name_);
                if (a != nullptr)
                {
                    old_value_ = a->value;
                }
            }
        }
        g.set_attr_value(id_, attr_name_, new_value_);
    }

    void SetAttributeValueCommand::revert(Graph& g)
    {
        if (old_value_.has_value())
        {
            g.set_attr_value(id_, attr_name_, *old_value_);
            old_value_.reset();
        }
    }

    bool SetAttributeValueCommand::try_merge(Command const& next)
    {
        auto const* other = dynamic_cast<SetAttributeValueCommand const*>(&next);
        if (other == nullptr or other->id_ != id_ or other->attr_name_ != attr_name_)
        {
            return false;
        }
        new_value_ = other->new_value_;
        return true;
    }

    void SetAttributeStagesCommand::apply(Graph& g)
    {
        if (not old_stages_.has_value())
        {
            Node const* live = g.find_node(id_);
            if (live != nullptr)
            {
                Attribute const* a = live->find_attr(attr_name_);
                if (a != nullptr)
                {
                    old_stages_ = a->stages;
                }
            }
        }
        g.set_attr_stages(id_, attr_name_, new_stages_);
    }

    void SetAttributeStagesCommand::revert(Graph& g)
    {
        if (old_stages_.has_value())
        {
            g.set_attr_stages(id_, attr_name_, *old_stages_);
            old_stages_.reset();
        }
    }

    bool SetAttributeStagesCommand::try_merge(Command const& next)
    {
        auto const* other = dynamic_cast<SetAttributeStagesCommand const*>(&next);
        if (other == nullptr or other->id_ != id_ or other->attr_name_ != attr_name_)
        {
            return false;
        }
        new_stages_ = other->new_stages_;
        return true;
    }

    void CompositeCommand::apply(Graph& g)
    {
        for (auto& child : children_)
        {
            child->apply(g);
        }
    }

    void CompositeCommand::revert(Graph& g)
    {
        for (auto it = children_.rbegin(); it != children_.rend(); ++it)
        {
            (*it)->revert(g);
        }
    }
}
