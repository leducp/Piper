#include "diagnostic_helpers.h"

namespace piper
{
    Diagnostic schema_error(std::string const& message)
    {
        Diagnostic d;
        d.kind    = Diagnostic::Kind::SchemaError;
        d.message = message;
        return d;
    }
}
