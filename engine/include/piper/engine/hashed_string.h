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

    // Borrowed name and its FNV-1a hash. Equality compares hashes only.
    // The Engine computes stage hashes at build() and profile/mode hashes
    // on set_profile(). A literal comparison can fold the literal's hash:
    //
    //     if (current_mode() == "loose") { ... }
    //
    // The source string must outlive this view. Engine stage names remain
    // valid until build(); profile and mode names may change on set_profile().
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

    std::ostream& operator<<(std::ostream& os, HashedString const& s);
}

#endif
