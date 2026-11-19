#ifndef PIPER_ENGINE_STEPS_CLAMP_STEP_H
#define PIPER_ENGINE_STEPS_CLAMP_STEP_H

#include <string>

#include "piper/engine/step.h"

#include "step/constant.h"  // parse_member_to<T>

namespace piper::engine::step
{
    // Saturates `in` to the closed range [min, max], using member bounds.
    template<typename T>
    class Clamp final : public Step
    {
    public:
        static std::string type_name() { return std::string("clamp") + type_suffix<T>(); }

        void declare_io() override
        {
            declare_input<T>("in");
            declare_output<T>("out", out_);
        }

        void on_mode_change(HashedString) override
        {
            min_ = parse_member_to<T>(member("min"));
            max_ = parse_member_to<T>(member("max"));
        }

        void compute(HashedString) override
        {
            T const v = input<T>("in");
            if (v < min_)
            {
                out_ = min_;
            }
            else if (v > max_)
            {
                out_ = max_;
            }
            else
            {
                out_ = v;
            }
        }

    private:
        T out_{};
        T min_{};
        T max_{};
    };
}

#endif
