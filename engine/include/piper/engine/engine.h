#ifndef PIPER_ENGINE_ENGINE_H
#define PIPER_ENGINE_ENGINE_H

#include <cstddef>
#include <stdint.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "piper/graph.h"
#include "piper/node.h"

#include "piper/engine/diagnostic.h"
#include "piper/engine/external_io.h"
#include "piper/engine/mode.h"
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

        // Optional human-readable label for this engine instance. The
        // engine never interprets it; it is a convenience for the host
        // to tell instances apart (e.g. many engines in a vector).
        // Survives build().
        std::string const& name() const { return name_; }
        void set_name(std::string name) { name_ = std::move(name); }

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

        // Switch the active mode profile by name. Nodes labeled
        // "disable" in the matching profile are skipped on subsequent
        // ticks; their outputs hold the value from their last compute()
        // (zero on a fresh build). An unknown name still updates what
        // current_mode() reports but disables nothing -- the host can
        // expose modes that are meaningful to step code without listing
        // them in the graph's mode_profiles.
        void set_mode(std::string_view name);
        Mode current_mode() const { return current_mode_; }

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
        std::unordered_map<std::string, step::Input<double>*>         input_double_;
        std::unordered_map<std::string, step::Input<int32_t>*>        input_int_;
        std::unordered_map<std::string, step::Output<float>*>         output_float_;
        std::unordered_map<std::string, step::Output<double>*>        output_double_;
        std::unordered_map<std::string, step::Output<int32_t>*>       output_int_;
        // Mode-aware execution. current_mode_name_ owns the active
        // profile name; current_mode_ is the {string_view, hash}
        // handle whose .name views into current_mode_name_. Every
        // IoBlock::current_mode points at &current_mode_ -- stable
        // across set_mode() calls. mode_labels_ caches each profile's
        // per_node table (cheaper than reaching back into the source
        // Graph). active_disabled_ is the subset of nodes whose label
        // in the active profile is "disable" -- consulted by tick_at
        // on the hot path.
        std::string                                                   current_mode_name_;
        Mode                                                          current_mode_{};
        std::unordered_map<std::string,
            std::unordered_map<piper::NodeId, std::string>>           mode_labels_;
        std::unordered_set<piper::NodeId>                             active_disabled_;
        std::string                                                   name_;
        bool                                                          ok_{false};

        // Internal direct-dispatch tick. Bypasses the hash compare
        // since play() already knows the index.
        void tick_at(std::size_t idx);
    };

    template<> step::Input<float>*          Engine::input<float>  (std::string_view name);
    template<> step::Input<double>*         Engine::input<double> (std::string_view name);
    template<> step::Input<int32_t>*        Engine::input<int32_t>(std::string_view name);
    template<> step::Output<float> const*   Engine::output<float> (std::string_view name) const;
    template<> step::Output<double> const*  Engine::output<double>(std::string_view name) const;
    template<> step::Output<int32_t> const* Engine::output<int32_t>(std::string_view name) const;
}

#endif
