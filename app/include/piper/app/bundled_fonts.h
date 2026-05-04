#ifndef PIPER_APP_BUNDLED_FONTS_H
#define PIPER_APP_BUNDLED_FONTS_H

#include <span>

namespace piper::studio
{
    struct BundledFont
    {
        char const*          name;
        unsigned char const* data;
        unsigned int         size;
    };

    std::span<BundledFont const> bundled_fonts();
}

#endif
