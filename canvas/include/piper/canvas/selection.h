#ifndef PIPER_CANVAS_SELECTION_H
#define PIPER_CANVAS_SELECTION_H

#include <cstddef>
#include <span>
#include <vector>

#include "piper/canvas/ids.h"

namespace piper::canvas
{
    // Order-preserving set of NodeId. ids() returns a span valid until
    // the next mutation; the framework relies on that to put a stable
    // span into Event::selection.
    class Selection
    {
    public:
        bool        empty() const { return ids_.empty(); }
        std::size_t size()  const { return ids_.size(); }

        bool contains(NodeId id) const;

        std::span<NodeId const> ids() const { return ids_; }

        // Each mutator returns true iff the selection actually changed.
        bool clear();
        bool set(std::span<NodeId const> ids);
        bool add(NodeId id);
        bool remove(NodeId id);
        bool toggle(NodeId id);

    private:
        std::vector<NodeId> ids_;
    };
}

#endif
