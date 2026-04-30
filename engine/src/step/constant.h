#ifndef PIPER_ENGINE_STEPS_CONSTANT_STEP_H
#define PIPER_ENGINE_STEPS_CONSTANT_STEP_H

#include <string>
#include <type_traits>

#include "piper/engine/step.h"

namespace piper::engine
{
    // Parse a Member string into T. Supported: float, double, int.
    // Add another branch here when a new built-in T is introduced.
    template<typename T>
    T parse_member_to(std::string const& s)
    {
        if constexpr (std::is_same_v<T, float>)
        {
            return std::stof(s);
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            return std::stod(s);
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            return std::stoi(s);
        }
        else
        {
            static_assert(sizeof(T) == 0, "parse_member_to<T>: unsupported T");
        }
    }

    namespace step
    {
        template<typename T>
        class Constant final : public Step
        {
        public:
            static std::string type_name() { return std::string("constant") + type_suffix<T>(); }

            void declare_io() override
            {
                declare_output<T>("out", out_);
                out_ = parse_member_to<T>(member("value"));
            }

            void compute(Stage) override {}

        private:
            T out_{};
        };
    }
}

#endif
