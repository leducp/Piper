#include "piper/engine/registry.h"

namespace piper::engine
{
    bool StepRegistry::add(std::string type, StepFactory factory)
    {
        if (factories_.find(type) != factories_.end())
        {
            return false;
        }
        factories_.emplace(std::move(type), std::move(factory));
        return true;
    }

    StepFactory const* StepRegistry::find(std::string_view type) const
    {
        auto it = factories_.find(std::string(type));
        if (it == factories_.end())
        {
            return nullptr;
        }
        return &it->second;
    }
}
