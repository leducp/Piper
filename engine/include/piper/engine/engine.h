#ifndef PIPER_ENGINE_ENGINE_H
#define PIPER_ENGINE_ENGINE_H

#include <cstddef>
#include <functional>
#include <map>
#include <stdint.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "piper/graph.h"
#include "piper/node.h"

#include "piper/engine/diagnostic.h"
#include "piper/engine/hashed_string.h"
#include "piper/engine/registry.h"
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
        // tick(). No-op if build() did not succeed or `stage` is not
        // one of the graph's stages. Comparison is by HashedString::id, so
        // hashing happens once on the caller side (or when the
        // HashedString is constructed from a string_view).
        void tick(HashedString stage);

        // Calls tick() once for each stage, in graph order. Iterates
        // pre-resolved indices internally -- no per-stage scan or
        // hashing.
        void play();

        // Switch the active profile by name. Each node whose mode changes
        // gets that mode's member values and on_mode_change(). Nodes whose
        // mode is "disable" in the matching profile are skipped on subsequent
        // ticks; their outputs hold the value from their last compute()
        // (zero on a fresh build). An unknown name still updates what
        // current_profile() reports but disables nothing -- the host can
        // expose profiles that are meaningful to step code without
        // listing them in the graph's profiles.
        void set_profile(std::string_view name);
        HashedString current_profile() const { return current_profile_; }

        // Look up step storage by the external attribute's value.
        // The returned pointer is non-owning and valid until the next build().
        // Returns nullptr if the name is missing or its C++ type differs from T.
        template<typename T>
        T* input(std::string_view name)
        {
            auto const* slot = find_external(external_inputs_, name);
            if (slot == nullptr)
            {
                return nullptr;
            }
            auto const* ref = std::any_cast<std::reference_wrapper<T>>(&slot->ref_any);
            if (ref == nullptr)
            {
                return nullptr;
            }
            return &ref->get();
        }

        template<typename T>
        T const* output(std::string_view name) const
        {
            auto const* slot = find_external(external_outputs_, name);
            if (slot == nullptr)
            {
                return nullptr;
            }
            auto const* ref = std::any_cast<std::reference_wrapper<T const>>(&slot->ref_any);
            if (ref == nullptr)
            {
                return nullptr;
            }
            return &ref->get();
        }

        // Pointer invalidated by build(). Returns nullptr for unknown ids.
        Step*       step_for(piper::NodeId id);
        Step const* step_for(piper::NodeId id) const;

        // Views into engine-owned storage; invalidated by build().
        std::vector<HashedString> const& stages() const;

    private:
        std::unordered_map<piper::NodeId, IoBlock>                    blocks_;
        std::vector<std::string>                                      stage_names_;   // owns the strings
        std::vector<HashedString>                                     stage_data_;    // {string_view into stage_names_, hash}
        std::vector<std::vector<piper::NodeId>>                       per_stage_order_;
        // Separate input and output maps, keyed by external name.
        // Slot addresses remain valid until blocks_ is cleared.
        // std::less<> allows string_view lookups without allocating.
        using ExternalTable = std::map<std::string, ExternalSlot const*, std::less<>>;
        ExternalTable                                                 external_inputs_;
        ExternalTable                                                 external_outputs_;
        // current_profile_ borrows current_profile_name_. IoBlocks point
        // to current_profile_, whose address stays stable across set_profile().
        // Profiles are copied from the graph; tick_at checks active_disabled_.
        std::string                                                   current_profile_name_;
        HashedString                                                  current_profile_{};
        std::unordered_map<std::string,
            std::unordered_map<piper::NodeId, std::string>>           profile_modes_;
        std::unordered_set<piper::NodeId>                             active_disabled_;
        std::string                                                   name_;
        bool                                                          ok_{false};

        // Internal direct-dispatch tick. Bypasses the hash compare
        // since play() already knows the index.
        void tick_at(std::size_t idx);

        // Loads block.mode_buf's member values and calls on_mode_change().
        static void apply_mode(IoBlock& block);

        static ExternalSlot const* find_external(ExternalTable const& table,
                                                 std::string_view name);
    };
}

#endif
