#ifndef PIPER_ENGINE_STEPS_PID_STEP_H
#define PIPER_ENGINE_STEPS_PID_STEP_H

#include <string>

#include "piper/engine/step.h"

namespace piper::engine::step
{
    // Discrete PID with gains as inputs (so they can be wired to
    // constants, presets, or arbitrary upstream signals).
    //
    // Two non-trivial choices:
    //
    // 1) Derivative on measurement, NOT on error. The error has a
    //    delta function each time the setpoint steps; differentiating
    //    it would blow up at any kd > 0. Differentiating the
    //    measurement instead leaves the derivative term as just
    //    "resist the plant's velocity" with no setpoint-step kick.
    //
    // 2) Derivative passes through a 1-pole low-pass at fc=16 Hz.
    //    At 1 kHz sampling, an unfiltered (m - m_prev)/dt amplifies
    //    every per-tick wiggle by 1000; the filter caps that gain so
    //    a small kd stays small. Tau is hardcoded; promote to a
    //    member if a demo needs to tune it.
    //
    //     out = kp*e + ki*integral - kd * lpf(d(measured)/dt)
    template<typename T>
    class Pid final : public Step
    {
    public:
        static std::string type_name() { return std::string("pid") + type_suffix<T>(); }

        void declare_io() override
        {
            declare_input<T>("setpoint");
            declare_input<T>("measured");
            declare_input<T>("kp");
            declare_input<T>("ki");
            declare_input<T>("kd");
            declare_output<T>("out", out_);
            first_tick_ = true;
        }

        void compute(Stage) override
        {
            T const sp = input<T>("setpoint");
            T const m  = input<T>("measured");
            T const kp = input<T>("kp");
            T const ki = input<T>("ki");
            T const kd = input<T>("kd");

            T const e = static_cast<T>(sp - m);
            integral_ = static_cast<T>(integral_ + e * static_cast<T>(tick_period));

            if (first_tick_)
            {
                prev_measured_  = m;
                d_filtered_     = T{};
                first_tick_     = false;
            }
            T const d_raw =
                static_cast<T>((m - prev_measured_) / static_cast<T>(tick_period));
            prev_measured_ = m;

            // 1-pole LPF on the derivative; alpha = dt / (tau + dt).
            constexpr double derivative_tau = 0.01;   // ~16 Hz cutoff
            constexpr double alpha = tick_period / (derivative_tau + tick_period);
            d_filtered_ = static_cast<T>(d_filtered_
                                          + static_cast<T>(alpha)
                                            * (d_raw - d_filtered_));

            out_ = static_cast<T>(kp * e + ki * integral_ - kd * d_filtered_);
        }

    private:
        static constexpr double tick_period = 0.001;

        T    out_{};
        T    integral_{};
        T    prev_measured_{};
        T    d_filtered_{};
        bool first_tick_{true};
    };
}

#endif
