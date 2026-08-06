#ifndef PIPER_ENGINE_STEPS_CONSTANT_STEP_H
#define PIPER_ENGINE_STEPS_CONSTANT_STEP_H

#include <cstddef>
#include <string>
#include <type_traits>

#include "piper/vec.h"

#include "piper/engine/step.h"

namespace piper::engine
{
    // Parse a Member string into T. Supported: the scalar widths in
    // piper::BuiltinScalars, plus Vec2<float> / Vec3<float>. Add
    // another branch here when a new built-in T is introduced.
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
        else if constexpr (std::is_integral_v<T> and std::is_signed_v<T>)
        {
            return static_cast<T>(std::stoll(s));
        }
        else if constexpr (std::is_integral_v<T> and std::is_unsigned_v<T>)
        {
            // stoull silently wraps a negative literal to a huge value
            // instead of throwing; saturate at 0 so a stray "-1" in a
            // member field cannot publish UINT_MAX.
            std::size_t const first = s.find_first_not_of(" \t\n\r\f\v");
            if (first != std::string::npos and s[first] == '-')
            {
                return T{0};
            }
            return static_cast<T>(std::stoull(s));
        }
        else if constexpr (std::is_same_v<T, piper::Vec2<float>>)
        {
            return piper::parse_vec2f(s);
        }
        else if constexpr (std::is_same_v<T, piper::Vec3<float>>)
        {
            return piper::parse_vec3f(s);
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
