#ifndef PIPER_COMMAND_STACK_H
#define PIPER_COMMAND_STACK_H

#include <cstddef>
#include <memory>
#include <vector>

#include "piper/command.h"

namespace piper
{
    class Graph;

    class CommandStack
    {
    public:
        // Applies the command immediately and pushes it. Within an open
        // group, the command first attempts to merge into the back of
        // the group (drag/keystroke coalescing). If unmerged, it joins
        // the group; otherwise it is appended to undo and clears redo.
        void push(std::unique_ptr<Command> cmd, Graph& g);

        // No-op when the respective stack is empty.
        void undo(Graph& g);
        void redo(Graph& g);

        bool can_undo() const { return not undo_.empty(); }
        bool can_redo() const { return not redo_.empty(); }

        // Composite commands count as one entry, not as their child count.
        std::size_t undo_size() const { return undo_.size(); }
        std::size_t redo_size() const { return redo_.size(); }

        // Cap the undo stack length. When undo.size() exceeds the cap,
        // entries are dropped from the front. 0 = unbounded (default).
        void        set_max_undo(std::size_t cap) { max_undo_ = cap; trim_undo(); }
        std::size_t max_undo() const              { return max_undo_; }

        // Group nesting uses a depth counter: callers may symmetrically
        // call open_group()/close_group() at multiple stack levels; only
        // the outermost close commits the group.
        void open_group();
        void close_group();

        bool group_open() const { return group_depth_ > 0; }

        void clear();

    private:
        void trim_undo();

        std::vector<std::unique_ptr<Command>> undo_;
        std::vector<std::unique_ptr<Command>> redo_;
        std::vector<std::unique_ptr<Command>> group_;

        int         group_depth_{0};
        std::size_t max_undo_{0};
    };

    // RAII helper: open a group on construction, close on destruction.
    // Use in editor code paths that may early-return so the stack never
    // ends up stuck with an open group.
    class ScopedGroup
    {
    public:
        explicit ScopedGroup(CommandStack& s) : stack_(s) { stack_.open_group(); }
        ~ScopedGroup()                                    { stack_.close_group(); }

        ScopedGroup(ScopedGroup const&)            = delete;
        ScopedGroup& operator=(ScopedGroup const&) = delete;

    private:
        CommandStack& stack_;
    };
}

#endif
