#include "piper/command_stack.h"

#include "piper/commands.h"
#include "piper/graph.h"

namespace piper
{
    void CommandStack::push(std::unique_ptr<Command> cmd, Graph& g)
    {
        cmd->apply(g);
        if (group_depth_ > 0)
        {
            // Try to coalesce drag/keystroke runs into one composite.
            if (not group_.empty() and group_.back()->try_merge(*cmd))
            {
                return;
            }
            group_.push_back(std::move(cmd));
        }
        else
        {
            undo_.push_back(std::move(cmd));
            redo_.clear();
            trim_undo();
        }
    }

    void CommandStack::undo(Graph& g)
    {
        if (undo_.empty())
        {
            return;
        }
        auto cmd = std::move(undo_.back());
        undo_.pop_back();
        cmd->revert(g);
        redo_.push_back(std::move(cmd));
    }

    void CommandStack::redo(Graph& g)
    {
        if (redo_.empty())
        {
            return;
        }
        auto cmd = std::move(redo_.back());
        redo_.pop_back();
        cmd->apply(g);
        undo_.push_back(std::move(cmd));
        trim_undo();
    }

    void CommandStack::open_group()
    {
        ++group_depth_;
    }

    void CommandStack::close_group()
    {
        if (group_depth_ == 0)
        {
            return;
        }
        --group_depth_;
        if (group_depth_ > 0)
        {
            return;
        }
        if (group_.empty())
        {
            return;
        }
        auto composite = std::make_unique<CompositeCommand>(std::move(group_));
        undo_.push_back(std::move(composite));
        redo_.clear();
        trim_undo();
    }

    void CommandStack::clear()
    {
        undo_.clear();
        redo_.clear();
        group_.clear();
        group_depth_ = 0;
    }

    void CommandStack::trim_undo()
    {
        if (max_undo_ == 0)
        {
            return;
        }
        while (undo_.size() > max_undo_)
        {
            undo_.erase(undo_.begin());
        }
    }
}
