#ifndef PIPER_ENGINE_ERRORS_H
#define PIPER_ENGINE_ERRORS_H

#include <system_error>
#include <type_traits>

namespace piper::engine
{
    enum class EngineError
    {
        UnknownProfile = 1,
    };

    std::error_category const& engine_category();
    std::error_code make_error_code(EngineError e);
}

template<>
struct std::is_error_code_enum<piper::engine::EngineError> : std::true_type {};

#endif
