#ifndef PIPER_APP_BUNDLED_LICENSES_H
#define PIPER_APP_BUNDLED_LICENSES_H

#include <span>

namespace piper::app
{
    struct BundledLicense
    {
        char const* name;
        char const* text;
    };

    std::span<BundledLicense const> bundled_licenses();
}

#endif
