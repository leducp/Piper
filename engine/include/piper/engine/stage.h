#ifndef PIPER_ENGINE_STAGE_H
#define PIPER_ENGINE_STAGE_H

#include <string_view>

namespace piper::engine
{
    // Valid only for the lifetime of the Engine that produced it.
    using Stage = std::string_view;
}

#endif
