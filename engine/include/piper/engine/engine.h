#ifndef PIPER_ENGINE_ENGINE_H
#define PIPER_ENGINE_ENGINE_H

#include <memory>
#include <vector>

#include "piper/graph.h"
#include "piper/node.h"

#include "piper/engine/diagnostic.h"
#include "piper/engine/registry.h"
#include "piper/engine/stage.h"

namespace piper::engine
{
    class Engine
    {
    public:
        struct BuildResult
        {
            bool                          ok{false};
            std::vector<BuildDiagnostic>  diagnostics;
        };

        Engine();
        ~Engine();
        Engine(Engine const&)            = delete;
        Engine& operator=(Engine const&) = delete;
        // After move-from, build()/tick() on the source are UB until
        // it is reassigned.
        Engine(Engine&&) noexcept;
        Engine& operator=(Engine&&) noexcept;

        BuildResult build(piper::Graph const& graph,
                          StepRegistry const& step_reg);

        // Exceptions thrown from a Step's compute() propagate out of
        // tick(). No-op if build() did not succeed or `current` is not
        // one of the graph's stages.
        void tick(Stage current);

        void tick_all_stages();

        // Pointer invalidated by build().
        Step*              step_for(piper::NodeId id);

        // Views into engine-owned storage; invalidated by build().
        std::vector<Stage> stages() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };
}

#endif
