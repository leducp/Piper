#ifndef PIPER_ERRORS_H
#define PIPER_ERRORS_H

#include <system_error>
#include <type_traits>

namespace piper
{
    enum class GraphError
    {
        UnknownNode = 1,
        UnknownAttribute,
        UnknownMode,
        InvalidModeName,    // empty or "disable"
        DuplicateMode,
        LastMode,           // a node keeps at least one mode
    };

    std::error_category const& graph_category();
    std::error_code make_error_code(GraphError e);
}

template<>
struct std::is_error_code_enum<piper::GraphError> : std::true_type {};

#endif
