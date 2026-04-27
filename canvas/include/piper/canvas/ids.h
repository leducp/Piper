#ifndef PIPER_CANVAS_IDS_H
#define PIPER_CANVAS_IDS_H

#include <cstdint>
#include <functional>

namespace piper::canvas
{
    // Opaque uint64 wrappers. Wrap explicitly via NodeId{ value }.
    struct NodeId
    {
        uint64_t v{0};

        constexpr bool operator==(NodeId other) const { return v == other.v; }
        constexpr bool operator!=(NodeId other) const { return not (*this == other); }
    };

    struct PinId
    {
        uint64_t v{0};

        constexpr bool operator==(PinId other) const { return v == other.v; }
        constexpr bool operator!=(PinId other) const { return not (*this == other); }
    };

    struct LinkId
    {
        uint64_t v{0};

        constexpr bool operator==(LinkId other) const { return v == other.v; }
        constexpr bool operator!=(LinkId other) const { return not (*this == other); }
    };

    inline constexpr NodeId invalid_node_id{0};
    inline constexpr PinId  invalid_pin_id{0};
    inline constexpr LinkId invalid_link_id{0};
}

namespace std
{
    template<>
    struct hash<piper::canvas::NodeId>
    {
        std::size_t operator()(piper::canvas::NodeId id) const noexcept
        {
            return std::hash<uint64_t>{}(id.v);
        }
    };

    template<>
    struct hash<piper::canvas::PinId>
    {
        std::size_t operator()(piper::canvas::PinId id) const noexcept
        {
            return std::hash<uint64_t>{}(id.v);
        }
    };

    template<>
    struct hash<piper::canvas::LinkId>
    {
        std::size_t operator()(piper::canvas::LinkId id) const noexcept
        {
            return std::hash<uint64_t>{}(id.v);
        }
    };
}

#endif
