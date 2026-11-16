#include "piper/engine/hashed_string.h"

namespace piper::engine
{
    std::ostream& operator<<(std::ostream& os, HashedString const& s)
    {
        return os << '"' << s.name << '"';
    }
}
