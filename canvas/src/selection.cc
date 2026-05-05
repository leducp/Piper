#include <algorithm>

#include "piper/canvas/selection.h"

namespace piper::canvas
{
    bool Selection::contains(NodeId id) const
    {
        return std::find(ids_.begin(), ids_.end(), id) != ids_.end();
    }

    bool Selection::clear()
    {
        if (ids_.empty())
        {
            return false;
        }
        ids_.clear();
        return true;
    }

    bool Selection::set(std::span<NodeId const> ids)
    {
        if (ids.size() == ids_.size()
            and std::equal(ids.begin(), ids.end(), ids_.begin()))
        {
            return false;
        }
        ids_.assign(ids.begin(), ids.end());
        return true;
    }

    bool Selection::add(NodeId id)
    {
        if (contains(id))
        {
            return false;
        }
        ids_.push_back(id);
        return true;
    }

    bool Selection::remove(NodeId id)
    {
        auto const it = std::find(ids_.begin(), ids_.end(), id);
        if (it == ids_.end())
        {
            return false;
        }
        ids_.erase(it);
        return true;
    }

    bool Selection::toggle(NodeId id)
    {
        auto const it = std::find(ids_.begin(), ids_.end(), id);
        if (it == ids_.end())
        {
            ids_.push_back(id);
        }
        else
        {
            ids_.erase(it);
        }
        return true;
    }
}
