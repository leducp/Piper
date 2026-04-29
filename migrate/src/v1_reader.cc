#include "piper/migrate/v1_reader.h"

#include <stdexcept>

namespace piper::migrate
{
    LoadResult read_v1(std::string_view,
                       NodeRegistry const&,
                       Options const&)
    {
        throw std::runtime_error("piper-migrate: V1 reader not yet implemented");
    }
}
