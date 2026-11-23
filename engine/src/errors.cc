#include <string>

#include "piper/engine/errors.h"

namespace piper::engine
{
    class EngineCategory final : public std::error_category
    {
    public:
        char const* name() const noexcept override { return "piper.engine"; }

        std::string message(int value) const override
        {
            switch (static_cast<EngineError>(value))
            {
                case EngineError::UnknownProfile: { return "the graph has no profile of that name"; }
            }
            return "unknown engine error";
        }
    };

    std::error_category const& engine_category()
    {
        static EngineCategory const category;
        return category;
    }

    std::error_code make_error_code(EngineError e)
    {
        return { static_cast<int>(e), engine_category() };
    }
}
