#ifndef PIPER_ENGINE_ENGINE_H
#define PIPER_ENGINE_ENGINE_H

#include <cstddef>
#include <stdint.h>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
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
        Engine() = default;

        // IoBlock::current_mode, Step::io_ and stage_data_ all point
        // into this object; copying or moving would leave them dangling.
        Engine(Engine const&)            = delete;
        Engine& operator=(Engine const&) = delete;
        Engine(Engine&&)                 = delete;
        Engine& operator=(Engine&&)      = delete;

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
        // set()/get() on the returned pointer in the hot path. The raw
        // pointer is non-owning and stays valid until the next build().
        // Returns nullptr if no external_input<T> / external_output<T>
        // node with that "name" Member exists in the graph.
        template<typename T> step::Input<T>*        input (std::string_view name);
        template<typename T> step::Output<T> const* output(std::string_view name) const;

        // Shared variants: the handle keeps its Step alive across
        // build(), but after a rebuild it is disconnected from the new
        // graph -- set()/get() touch only the old, unscheduled Step.
        // nullptr when the name is absent.
        template<typename T> std::shared_ptr<step::Input<T>>  input_shared (std::string_view name) const;
        template<typename T> std::shared_ptr<step::Output<T>> output_shared(std::string_view name) const;

        // Pointer invalidated by build(). Returns nullptr for unknown ids.
        Step*       step_for(piper::NodeId id);
        Step const* step_for(piper::NodeId id) const;

        // Shared variant: outlives build() but is disconnected from
        // the new graph after a rebuild. nullptr for unknown ids.
        std::shared_ptr<Step> step_shared(piper::NodeId id) const;

        // Views into engine-owned storage; invalidated by build().
        std::vector<Stage> const& stages() const;

    private:
        std::unordered_map<piper::NodeId, IoBlock>                    blocks_;
        std::vector<std::string>                                      stage_names_;   // owns the strings
        std::vector<Stage>                                            stage_data_;    // {string_view into stage_names_, hash}
        std::vector<std::vector<piper::NodeId>>                       per_stage_order_;
        // per_stage_order_ resolved to IoBlock pointers at the end of
        // build(); valid because unordered_map nodes are address-stable.
        std::vector<std::vector<IoBlock*>>                            per_stage_blocks_;
        // Aliasing shared_ptrs onto each block's shared_ptr<Step>, so
        // a handle handed out via *_shared keeps its Step alive.
        std::unordered_map<std::string, std::shared_ptr<step::Input<float>>>    input_float_;
        std::unordered_map<std::string, std::shared_ptr<step::Input<double>>>   input_double_;
        std::unordered_map<std::string, std::shared_ptr<step::Input<int32_t>>>  input_int_;
        std::unordered_map<std::string, std::shared_ptr<step::Output<float>>>   output_float_;
        std::unordered_map<std::string, std::shared_ptr<step::Output<double>>>  output_double_;
        std::unordered_map<std::string, std::shared_ptr<step::Output<int32_t>>> output_int_;
        // Mode-aware execution. current_mode_name_ owns the active
        // profile name; current_mode_ is the {string_view, hash}
        // handle whose .name views into current_mode_name_. Every
        // IoBlock::current_mode points at &current_mode_ -- stable
        // across set_mode() calls. mode_labels_ caches each profile's
        // per_node table (cheaper than reaching back into the source
        // Graph). Nodes labeled "disable" in the active profile carry
        // IoBlock::disabled = true -- consulted by tick_at on the hot
        // path.
        std::string                                                   current_mode_name_;
        Mode                                                          current_mode_{};
        std::unordered_map<std::string,
            std::unordered_map<piper::NodeId, std::string>>           mode_labels_;
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

    template<> std::shared_ptr<step::Input<float>>    Engine::input_shared<float>   (std::string_view name) const;
    template<> std::shared_ptr<step::Input<double>>   Engine::input_shared<double>  (std::string_view name) const;
    template<> std::shared_ptr<step::Input<int32_t>>  Engine::input_shared<int32_t> (std::string_view name) const;
    template<> std::shared_ptr<step::Output<float>>   Engine::output_shared<float>  (std::string_view name) const;
    template<> std::shared_ptr<step::Output<double>>  Engine::output_shared<double> (std::string_view name) const;
    template<> std::shared_ptr<step::Output<int32_t>> Engine::output_shared<int32_t>(std::string_view name) const;
}

#endif
