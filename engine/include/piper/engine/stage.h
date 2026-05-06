#ifndef PIPER_ENGINE_STAGE_H
#define PIPER_ENGINE_STAGE_H

#include <stdint.h>

#include <string_view>

namespace piper::engine
{
    // Constexpr FNV-1a 64-bit hash. Deterministic across builds and
    // platforms, so a Step author can compare against a compile-time
    // hash of a literal name -- no runtime hashing on the hot path.
    // Used for stage names, mode names, and per-node mode labels:
    //     static constexpr auto CONTROL = hash_name("control");
    //     void compute(Stage s) override { if (s.id == CONTROL) ... }
    constexpr uint64_t hash_name(std::string_view name)
    {
        uint64_t h = 0xcbf29ce484222325ULL;
        for (char c : name)
        {
            h ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
            h *= 0x100000001b3ULL;
        }
        return h;
    }

    // Stage handle bundled with the FNV-1a hash of `name`. Engine
    // pre-computes both at build() and passes the struct by value
    // through compute(). `name` is borrowed; valid for the lifetime
    // of the Engine that produced it.
    struct Stage
    {
        std::string_view name;
        uint64_t         id{0};

        constexpr Stage() = default;
        constexpr Stage(std::string_view n)
            : name{n}, id{hash_name(n)} {}
        constexpr Stage(char const* n)
            : Stage{std::string_view{n}} {}

        constexpr bool operator==(Stage const& other) const { return id == other.id; }
        constexpr bool operator!=(Stage const& other) const { return id != other.id; }
    };
}

#endif
