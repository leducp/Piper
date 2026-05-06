#ifndef PIPER_ENGINE_STEPS_ABS_STEP_H
#define PIPER_ENGINE_STEPS_ABS_STEP_H

#include <cmath>
#include <cstdlib>
#include <string>
#include <type_traits>

#include "piper/engine/step.h"

namespace piper::engine::step
{
    template<typename T>
    class Abs final : public Step
    {
    public:
        static std::string type_name() { return std::string("abs") + type_suffix<T>(); }

        void declare_io() override
        {
            declare_input<T>("in");
            declare_output<T>("out", out_);
        }

        void compute(Stage) override
        {
            T const v = input<T>("in");
            if constexpr (std::is_floating_point_v<T>)
            {
                out_ = std::fabs(v);
            }
            else if (v < T{0})
            {
                out_ = static_cast<T>(-v);
            }
            else
            {
                out_ = v;
            }
        }

    private:
        T out_{};
    };
}

#endif
