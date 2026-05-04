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
            // Snapshot incident links -- Graph::remove_node cascades.
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

    void SetNodeNoteCommand::apply(Graph& g)
    {
        if (not old_note_.has_value())
        {
            Node const* live = g.find_node(id_);
            if (live != nullptr)
            {
                old_note_ = live->note;
            }
        }
        g.set_node_note(id_, new_note_);
    }

    void SetNodeNoteCommand::revert(Graph& g)
    {
        if (old_note_.has_value())
        {
            g.set_node_note(id_, *old_note_);
            old_note_.reset();
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

    // ---- Stage CRUD ----

    void AddStageCommand::apply(Graph& g)
    {
        g.add_stage(stage_);
    }

    void AddStageCommand::revert(Graph& g)
    {
        g.remove_stage(stage_.name);
    }

    void RemoveStageCommand::apply(Graph& g)
    {
        if (not snapshot_.has_value())
        {
            for (std::size_t i = 0; i < g.stages().size(); ++i)
            {
                if (g.stages()[i].name == name_)
                {
                    snapshot_       = g.stages()[i];
                    original_index_ = i;
                    break;
                }
            }
        }
        if (snapshot_.has_value())
        {
            g.remove_stage(name_);
        }
    }

    void RemoveStageCommand::revert(Graph& g)
    {
        if (snapshot_.has_value())
        {
            g.insert_stage_at(*snapshot_, original_index_);
        }
    }

    void MoveStageCommand::apply(Graph& g)
    {
        if (not captured_)
        {
            captured_ = true;
            before_order_.clear();
            before_order_.reserve(g.stages().size());
            for (auto const& s : g.stages())
            {
                before_order_.push_back(s.name);
            }
        }
        g.move_stage_to(name_, target_);
    }

    void MoveStageCommand::revert(Graph& g)
    {
        if (captured_)
        {
            g.set_stages_order(before_order_);
        }
    }

    void SetStageColorCommand::apply(Graph& g)
    {
        if (not old_color_.has_value())
        {
            for (auto const& s : g.stages())
            {
                if (s.name == name_)
                {
                    old_color_ = s.color;
                    break;
                }
            }
        }
        g.set_stage_color(name_, new_color_);
    }

    void SetStageColorCommand::revert(Graph& g)
    {
        if (old_color_.has_value())
        {
            g.set_stage_color(name_, *old_color_);
            old_color_.reset();
        }
    }

    // ---- Annotation CRUD ----

    void AddAnnotationCommand::apply(Graph& g)
    {
        if (first_apply_)
        {
            first_apply_ = false;
            snapshot_.id = g.add_annotation(snapshot_);
            return;
        }
        g.insert_annotation(snapshot_);
    }

    void AddAnnotationCommand::revert(Graph& g)
    {
        if (snapshot_.id == invalid_annotation_id)
        {
            return;
        }
        Annotation const* live = g.find_annotation(snapshot_.id);
        if (live != nullptr)
        {
            snapshot_ = *live;
        }
        g.remove_annotation(snapshot_.id);
    }

    void DeleteAnnotationCommand::apply(Graph& g)
    {
        if (not snapshot_.has_value())
        {
            for (std::size_t i = 0; i < g.annotations().size(); ++i)
            {
                if (g.annotations()[i].id == id_)
                {
                    snapshot_       = g.annotations()[i];
                    original_index_ = i;
                    break;
                }
            }
        }
        if (snapshot_.has_value())
        {
            g.remove_annotation(id_);
        }
    }

    void DeleteAnnotationCommand::revert(Graph& g)
    {
        if (snapshot_.has_value())
        {
            g.insert_annotation_at(*snapshot_, original_index_);
        }
    }

    void SetAnnotationTextCommand::apply(Graph& g)
    {
        if (not old_text_.has_value())
        {
            Annotation const* a = g.find_annotation(id_);
            if (a != nullptr)
            {
                old_text_ = a->text;
            }
        }
        g.set_annotation_text(id_, new_text_);
    }

    void SetAnnotationTextCommand::revert(Graph& g)
    {
        if (old_text_.has_value())
        {
            g.set_annotation_text(id_, *old_text_);
            old_text_.reset();
        }
    }

    // ---- Mode profile CRUD ----

    void AddModeProfileCommand::apply(Graph& g)
    {
        g.add_mode_profile(profile_);
    }

    void AddModeProfileCommand::revert(Graph& g)
    {
        g.remove_mode_profile(profile_.name);
    }

    void RemoveModeProfileCommand::apply(Graph& g)
    {
        if (not snapshot_.has_value())
        {
            for (std::size_t i = 0; i < g.mode_profiles().size(); ++i)
            {
                if (g.mode_profiles()[i].name == name_)
                {
                    snapshot_       = g.mode_profiles()[i];
                    original_index_ = i;
                    break;
                }
            }
        }
        if (snapshot_.has_value())
        {
            g.remove_mode_profile(name_);
        }
    }

    void RemoveModeProfileCommand::revert(Graph& g)
    {
        if (snapshot_.has_value())
        {
            g.insert_mode_profile_at(*snapshot_, original_index_);
        }
    }

    void SetNodeModeLabelCommand::apply(Graph& g)
    {
        if (not captured_)
        {
            captured_ = true;
            for (auto const& mp : g.mode_profiles())
            {
                if (mp.name != profile_)
                {
                    continue;
                }
                auto it = mp.per_node.find(node_id_);
                if (it != mp.per_node.end())
                {
                    old_label_ = it->second;
                }
                break;
            }
        }
        g.set_node_mode_label(profile_, node_id_, new_label_);
    }

    void SetNodeModeLabelCommand::revert(Graph& g)
    {
        std::string const restore = old_label_.value_or(std::string{});
        g.set_node_mode_label(profile_, node_id_, restore);
    }
}
