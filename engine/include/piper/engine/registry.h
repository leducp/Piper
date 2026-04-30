#ifndef PIPER_ENGINE_REGISTRY_H
#define PIPER_ENGINE_REGISTRY_H

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include "piper/engine/step.h"

namespace piper::engine
{
    using StepFactory = std::function<std::shared_ptr<Step>()>;

    // Registration must happen single-threaded at startup. Concurrent
    // calls to add() may corrupt the underlying table.
    class StepRegistry
    {
    public:
        // Duplicate type name: existing entry kept, returns false.
        bool add(std::string type, StepFactory factory);

        StepFactory const* find(std::string_view type) const;

        std::size_t size()  const { return factories_.size(); }
        bool        empty() const { return factories_.empty(); }

    private:
        std::unordered_map<std::string, StepFactory> factories_;
    };
}

#endif
