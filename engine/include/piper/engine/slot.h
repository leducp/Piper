#ifndef PIPER_ENGINE_SLOT_H
#define PIPER_ENGINE_SLOT_H

#include <stdint.h>

#include <string_view>

#include "piper/engine/stage.h"

namespace piper::engine
{
    // Constexpr FNV-1a 64-bit hash of a slot name. Identical algorithm
    // to hash_stage(); kept as a separate name to express intent at
    // call sites. A Step author writes:
    //     static constexpr auto SENSE = hash_slot("sense");
    //     void compute(Slot s) override { if (s.id == SENSE) ... }
    constexpr uint64_t hash_slot(std::string_view name)
    {
        return hash_stage(name);
    }

    // Slot handle bundled with the FNV-1a hash of `name`. Engine
    // pre-computes both at build() and passes the struct by value
    // through Step::compute(). `name` is borrowed; valid for the
    // lifetime of the Engine that produced it.
    //
    // Slots are the step type's local computation phases (e.g.
    // "sense", "control"). The user binds each slot to a graph stage
    // per node in the editor. Step code never refers to graph stage
    // names -- only its own slot names.
    struct Slot
    {
        std::string_view name;
        uint64_t         id{0};

        constexpr Slot() = default;
        constexpr Slot(std::string_view n)
            : name{n}, id{hash_slot(n)} {}
        constexpr Slot(char const* n)
            : Slot{std::string_view{n}} {}
        // For pre-hashed entries on the dispatch hot path.
        constexpr Slot(std::string_view n, uint64_t pre_hashed)
            : name{n}, id{pre_hashed} {}

        constexpr bool operator==(Slot const& other) const { return id == other.id; }
        constexpr bool operator!=(Slot const& other) const { return id != other.id; }
    };
}

#endif
