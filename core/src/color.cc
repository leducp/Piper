#include "piper/color.h"

#include <cmath>

namespace piper
{
    rgba pastel_from_hue_index(int idx, float saturation, float value)
    {
        constexpr float golden = 0.61803398875f;
        float hue = std::fmod(float(idx) * golden, 1.0f);
        if (hue < 0.0f)
        {
            hue += 1.0f;
        }

        // Standard HSV→RGB. h in [0,1), s/v in [0,1].
        float const h6 = hue * 6.0f;
        int const   sector = int(std::floor(h6)) % 6;
        float const f = h6 - std::floor(h6);
        float const p = value * (1.0f - saturation);
        float const q = value * (1.0f - saturation * f);
        float const t = value * (1.0f - saturation * (1.0f - f));

        float r = value;
        float g = value;
        float b = value;
        switch (sector)
        {
            case 0:
            {
                r = value; g = t;     b = p;
                break;
            }
            case 1:
            {
                r = q;     g = value; b = p;
                break;
            }
            case 2:
            {
                r = p;     g = value; b = t;
                break;
            }
            case 3:
            {
                r = p;     g = q;     b = value;
                break;
            }
            case 4:
            {
                r = t;     g = p;     b = value;
                break;
            }
            case 5:
            {
                r = value; g = p;     b = q;
                break;
            }
            default:
            {
                break;
            }
        }

        auto const to_byte = [](float c)
        {
            float clamped = c;
            if (clamped < 0.0f)
            {
                clamped = 0.0f;
            }
            if (clamped > 1.0f)
            {
                clamped = 1.0f;
            }
            return uint8_t(clamped * 255.0f + 0.5f);
        };
        return rgba::from_components(to_byte(r), to_byte(g), to_byte(b), 0xFF);
    }
}
