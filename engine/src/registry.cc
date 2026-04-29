#include "piper/engine/registry.h"

namespace piper::engine
{
    bool StepRegistry::add(std::string type, StepFactory factory)
    {
        if (by_type_.find(type) != by_type_.end())
        {
            return false;
        }
        by_type_.emplace(std::move(type), std::move(factory));
        return true;
    }

    StepFactory const* StepRegistry::find(std::string_view type) const
    {
        auto it = by_type_.find(std::string(type));
        if (it == by_type_.end())
        {
            return nullptr;
        }
        return &it->second;
    }
}
