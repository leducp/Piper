#ifndef PIPER_CORE_DIAGNOSTIC_HELPERS_H
#define PIPER_CORE_DIAGNOSTIC_HELPERS_H

#include <string>

#include "piper/diagnostic.h"

namespace piper
{
    // Internal to core's source files. Not exposed in include/.
    Diagnostic schema_error(std::string const& message);
}

#endif
