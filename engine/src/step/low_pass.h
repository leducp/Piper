#ifndef PIPER_ENGINE_STEPS_LOW_PASS_STEP_H
#define PIPER_ENGINE_STEPS_LOW_PASS_STEP_H

#include <numbers>
#include <string>

#include "piper/engine/step.h"

namespace piper::engine::step
{
    template<typename T>
    class LowPass final : public Step
    {
    public:
        static std::string type_name() { return std::string("low_pass") + type_suffix<T>(); }

        void declare_io() override
        {
            declare_input<T>("in");
            declare_input<T>("dt_in", /*optional=*/true);
            declare_output<T>("out", out_);
            cutoff_    = std::stod(member("cutoff"));
            dt_member_ = std::stod(member("dt"));
        }

        void compute(Stage) override
        {
            T const in       = input<T>("in");
            double const dt    = resolve_dt<T>(dt_member_);
            double const tau   = 1.0 / (2.0 * std::numbers::pi_v<double> * cutoff_);
            double const alpha = dt / (tau + dt);
            double const next  = static_cast<double>(out_)
                               + alpha * (static_cast<double>(in) - static_cast<double>(out_));
            out_ = static_cast<T>(next);
        }

    private:
        T      out_{};
        double cutoff_{10.0};
        double dt_member_{0.001};
    };
}

#endif
