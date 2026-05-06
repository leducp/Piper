#ifndef PIPER_ENGINE_MODE_H
#define PIPER_ENGINE_MODE_H

#include <stdint.h>

#include <ostream>
#include <string_view>

#include "piper/engine/stage.h"  // hash_name

namespace piper::engine
{
    // Name + FNV-1a hash, used for both the active mode profile name
    // and per-node mode labels. The Engine pre-computes the hash on
    // set_mode() and at build() so the hot path never hashes; users
    // compare with a string literal -- the constexpr ctor folds the
    // literal hash at -O1+, leaving a single uint64 compare:
    //
    //     if (current_label() == "loose") { ... }
    //
    // `name` is borrowed; valid for the lifetime of the IoBlock /
    // Engine that produced it.
    struct Mode
    {
        std::string_view name;
        uint64_t         id{0};

        constexpr Mode() = default;
        constexpr Mode(std::string_view n) : name{n}, id{hash_name(n)} {}
        constexpr Mode(char const* n) : Mode{std::string_view{n}} {}

        constexpr bool operator==(Mode const& o) const { return id == o.id; }
        constexpr bool operator!=(Mode const& o) const { return id != o.id; }

        // Convenience: did the engine clear this handle? Empty means
        // "no active mode" (Engine) or "no label for this node in the
        // active profile" (Step).
        constexpr bool empty() const { return name.empty(); }
    };

    inline std::ostream& operator<<(std::ostream& os, Mode const& m)
    {
        return os << "Mode(\"" << m.name << "\")";
    }
}

#endif
