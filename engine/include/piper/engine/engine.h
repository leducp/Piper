#ifndef PIPER_ENGINE_ENGINE_H
#define PIPER_ENGINE_ENGINE_H

#include <cstddef>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "piper/graph.h"
#include "piper/node.h"

#include "piper/engine/diagnostic.h"
#include "piper/engine/external_io.h"
#include "piper/engine/registry.h"
#include "piper/engine/stage.h"
#include "piper/engine/step.h"

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

        BuildResult build(piper::Graph const& graph,
                          StepRegistry const& step_reg);

        // Exceptions thrown from a Step's compute() propagate out of
        // tick(). No-op if build() did not succeed or `current` is not
        // one of the graph's stages. Comparison is by Stage::id, so
        // hashing happens once on the caller side (or on Stage::Stage
        // when constructed from a string_view).
        void tick(Stage current);

        // Calls tick() once for each stage, in graph order. Iterates
        // pre-resolved indices internally -- no per-stage scan or
        // hashing.
        void play();

        // External I/O lookup: resolve once at HAL setup, then call
        // set()/get() on the returned pointer in the hot path. The
        // pointer is non-owning and stays valid until the next build().
        // Returns nullptr if no external_input<T> / external_output<T>
        // node with that "name" Member exists in the graph.
        template<typename T> step::Input<T>*        input (std::string_view name);
        template<typename T> step::Output<T> const* output(std::string_view name) const;

        // Pointer invalidated by build(). Returns nullptr for unknown ids.
        Step*       step_for(piper::NodeId id);
        Step const* step_for(piper::NodeId id) const;

        // Views into engine-owned storage; invalidated by build().
        std::vector<Stage> const& stages() const;

    private:
        std::unordered_map<piper::NodeId, IoBlock>                    blocks_;
        std::vector<std::string>                                      stage_names_;   // owns the strings
        std::vector<Stage>                                            stage_data_;    // {string_view into stage_names_, hash}
        std::vector<std::vector<piper::NodeId>>                       per_stage_order_;
        std::unordered_map<std::string, step::Input<float>*>          input_float_;
        std::unordered_map<std::string, step::Input<int32_t>*>        input_int_;
        std::unordered_map<std::string, step::Output<float>*>         output_float_;
        std::unordered_map<std::string, step::Output<int32_t>*>       output_int_;
        bool                                                          ok_{false};

        // Internal direct-dispatch tick. Bypasses the hash compare
        // since play() already knows the index.
        void tick_at(std::size_t idx);
    };

    template<> step::Input<float>*          Engine::input<float>  (std::string_view name);
    template<> step::Input<int32_t>*        Engine::input<int32_t>(std::string_view name);
    template<> step::Output<float> const*   Engine::output<float> (std::string_view name) const;
    template<> step::Output<int32_t> const* Engine::output<int32_t>(std::string_view name) const;
}

#endif
