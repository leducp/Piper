#include <string>

#include "piper/errors.h"

namespace piper
{
    class GraphCategory final : public std::error_category
    {
    public:
        char const* name() const noexcept override { return "piper.graph"; }

        std::string message(int value) const override
        {
            switch (static_cast<GraphError>(value))
            {
                case GraphError::UnknownNode:      { return "unknown node"; }
                case GraphError::UnknownAttribute: { return "unknown attribute"; }
                case GraphError::UnknownMode:      { return "unknown mode"; }
                case GraphError::InvalidModeName:  { return "a mode name cannot be empty or \"disable\""; }
                case GraphError::DuplicateMode:    { return "the node already has this mode"; }
                case GraphError::LastMode:         { return "a node keeps at least one mode"; }
                case GraphError::InvalidModeList:  { return "a mode list must be non-empty, without empty names, \"disable\" or duplicates"; }
                case GraphError::UnknownProfile:   { return "unknown profile"; }
                case GraphError::DefaultProfile:   { return "the default profile cannot be removed"; }
            }
            return "unknown graph error";
        }
    };

    std::error_category const& graph_category()
    {
        static GraphCategory const category;
        return category;
    }

    std::error_code make_error_code(GraphError e)
    {
        return { static_cast<int>(e), graph_category() };
    }
}
