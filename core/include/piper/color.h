#ifndef PIPER_COLOR_H
#define PIPER_COLOR_H

#include <cstdint>

namespace piper
{
    // Packed as 0xRRGGBBAA -- red in the high byte, alpha in the low byte.
    struct rgba
    {
        uint32_t value{0};

        constexpr uint8_t r() const { return uint8_t((value >> 24) & 0xFFu); }
        constexpr uint8_t g() const { return uint8_t((value >> 16) & 0xFFu); }
        constexpr uint8_t b() const { return uint8_t((value >>  8) & 0xFFu); }
        constexpr uint8_t a() const { return uint8_t( value        & 0xFFu); }

        constexpr rgba with_alpha(uint8_t alpha) const
        {
            return rgba{ (value & 0xFFFFFF00u) | uint32_t(alpha) };
        }

        static constexpr rgba from_components(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
        {
            return rgba{
                  (uint32_t(r) << 24)
                | (uint32_t(g) << 16)
                | (uint32_t(b) <<  8)
                |  uint32_t(a)
            };
        }

        constexpr bool operator==(rgba other) const { return value == other.value; }
        constexpr bool operator!=(rgba other) const { return value != other.value; }
    };

    // Pastel color from a stable integer index, using golden-ratio
    // hue cycling. Successive indices land on well-spaced hues, so
    // neighboring types stay easy to distinguish; saturation/value
    // are pulled toward pastel for general readability.
    //
    // Persist `idx` (a small int) per type -- not the resulting color
    // -- so a reload reproduces the same colors deterministically and
    // renaming a type does not shift its color. Theme overrides take
    // precedence at the application level.
    rgba pastel_from_hue_index(int   idx,
                               float saturation = 0.45f,
                               float value      = 0.92f);
}

#endif
