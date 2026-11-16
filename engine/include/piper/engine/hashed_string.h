#ifndef PIPER_ENGINE_HASHED_STRING_H
#define PIPER_ENGINE_HASHED_STRING_H

#include <stdint.h>

#include <ostream>
#include <string_view>

namespace piper::engine
{
    // Constexpr FNV-1a 64-bit hash. Deterministic across builds and
    // platforms, so a Step author can compare against a compile-time
    // hash of a literal name -- no runtime hashing on the hot path:
    //     static constexpr auto CONTROL = hash_name("control");
    //     void compute(HashedString stage) override { if (stage.id == CONTROL) ... }
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

    // Name + FNV-1a hash of that name. Used for stage names, the active
    // profile name and each step's mode. The Engine pre-computes the
    // hash at build() and on set_profile(), so the hot path never
    // hashes; comparing with a string literal folds the literal hash at
    // compile time, leaving a single uint64 compare:
    //
    //     if (current_mode() == "loose") { ... }
    //
    // `name` is borrowed: valid for the lifetime of the Engine that
    // produced it.
    struct HashedString
    {
        std::string_view name;
        uint64_t         id{0};

        constexpr HashedString() = default;
        constexpr HashedString(std::string_view n) : name{n}, id{hash_name(n)} {}
        constexpr HashedString(char const* n) : HashedString{std::string_view{n}} {}

        constexpr bool operator==(HashedString const& o) const { return id == o.id; }
        constexpr bool operator!=(HashedString const& o) const { return id != o.id; }

        constexpr bool empty() const { return name.empty(); }
    };

    inline std::ostream& operator<<(std::ostream& os, HashedString const& s)
    {
        return os << '"' << s.name << '"';
    }
}

#endif
