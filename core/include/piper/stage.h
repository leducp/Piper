#ifndef PIPER_STAGE_H
#define PIPER_STAGE_H

#include <string>

#include "piper/color.h"

namespace piper
{
    struct Stage
    {
        std::string name;
        rgba        color{0xFFFFFFFFu};
    };
}

#endif
