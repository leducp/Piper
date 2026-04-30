#ifndef PIPER_ENGINE_STEPS_SIN_WAVE_STEP_H
#define PIPER_ENGINE_STEPS_SIN_WAVE_STEP_H

#include <cmath>
#include <numbers>
#include <string>

#include "piper/engine/step.h"

namespace piper::engine::step
{
    // Phase advances by tick_period per compute() call. If the node
    // is active on N stages, the wave steps N times per period -- by
    // design, so multi-stage tagging scales the effective rate.
    template<typename T>
    class SinWave final : public Step
    {
    public:
        static std::string type_name() { return std::string("sin_wave") + type_suffix<T>(); }

        void declare_io() override
        {
            declare_output<T>("out", out_);
            frequency_ = std::stod(member("frequency"));
            amplitude_ = std::stod(member("amplitude"));
            phase_     = std::stod(member("phase"));
        }

        void compute(Stage) override
        {
            double const angle = 2.0 * std::numbers::pi_v<double> * frequency_ * t_ + phase_;
            out_ = static_cast<T>(amplitude_ * std::sin(angle));
            t_ += tick_period;
        }

    private:
        static constexpr double tick_period = 0.001;

        T      out_{};
        double t_{0.0};
        double frequency_{1.0};
        double amplitude_{1.0};
        double phase_{0.0};
    };
}

#endif
