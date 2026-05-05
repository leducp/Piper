#ifndef PIPER_COMMAND_H
#define PIPER_COMMAND_H

namespace piper
{
    class Graph;

    // Contract:
    //   apply() must be called before the first revert().
    //   revert() must tolerate a missing target (e.g. another command
    //     deleted it later); implementations no-op gracefully.
    //   apply() called twice without an intervening revert() must NOT
    //     overwrite captured "old" state (use the std::optional guard
    //     pattern in concrete commands).
    class Command
    {
    public:
        virtual ~Command() = default;

        virtual void apply(Graph& g)  = 0;
        virtual void revert(Graph& g) = 0;

        // Coalesces consecutive same-target commands within an open group.
        // Default returns false (no merge). Override to absorb `next` --
        // CommandStack discards `next` on a successful merge.
        virtual bool try_merge(Command const& next) { (void)next; return false; }

        // Returns a stable per-type pointer used by `try_merge` to
        // verify the dynamic type without RTTI: two commands compare
        // equal here only if they share the same most-derived type.
        // Default returns nullptr; non-mergeable commands inherit it.
        // Concrete merge-capable commands return the address of a
        // function-local static so each type has its own sentinel.
        virtual void const* merge_tag() const { return nullptr; }
    };
}

#endif
