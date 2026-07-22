#ifndef PIPER_ENGINE_STEPS_PID_STEP_H
#define PIPER_ENGINE_STEPS_PID_STEP_H

#include <string>

#include "piper/engine/step.h"

namespace piper::engine::step
{
    // Discrete PID with gains as inputs (so they can be wired to
    // constants, presets, or arbitrary upstream signals).
    //
    // Three non-trivial choices:
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
    // 3) Anti-windup via output saturation + conditional integration.
    //    out is clamped to [out_min, out_max] (members; default
    //    effectively unbounded). When the unsaturated controller
    //    output would land outside the limits, the integral is NOT
    //    updated this tick -- the integrator can't keep growing while
    //    the actuator is pinned. Simple, robust, sufficient for the
    //    bundled demos.
    //
    //     out = clamp(kp*e + ki*integral - kd * lpf(d_m/dt),
    //                 out_min, out_max)
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
            declare_input<T>("dt_in", /*optional=*/true);
            declare_output<T>("out", out_);
            dt_member_  = std::stod(member("dt"));
            out_min_    = std::stod(member("out_min"));
            out_max_    = std::stod(member("out_max"));
            first_tick_ = true;
        }

        void compute(Stage) override
        {
            T const sp = input<T>("setpoint");
            T const m  = input<T>("measured");
            T const kp = input<T>("kp");
            T const ki = input<T>("ki");
            T const kd = input<T>("kd");

            double const dt = resolve_dt<T>(dt_member_);

            T const e = static_cast<T>(sp - m);

            if (first_tick_)
            {
                prev_measured_  = m;
                d_filtered_     = T{};
                first_tick_     = false;
            }
            T const d_raw =
                static_cast<T>((m - prev_measured_) / static_cast<T>(dt));
            prev_measured_ = m;

            // 1-pole LPF on the derivative; alpha = dt / (tau + dt).
            double const derivative_tau = 10.0 * dt;
            double const alpha = dt / (derivative_tau + dt);
            d_filtered_ = static_cast<T>(d_filtered_
                                          + static_cast<T>(alpha)
                                            * (d_raw - d_filtered_));

            // Tentative integral + tentative output. If the result
            // saturates, freeze the integral so it can't keep growing
            // against an output limit (anti-windup).
            T const next_integral =
                static_cast<T>(integral_ + e * static_cast<T>(dt));
            T const u_unsat =
                static_cast<T>(kp * e + ki * next_integral - kd * d_filtered_);

            T u = u_unsat;
            bool saturated = false;
            if (u_unsat > static_cast<T>(out_max_))
            {
                u         = static_cast<T>(out_max_);
                saturated = true;
            }
            else if (u_unsat < static_cast<T>(out_min_))
            {
                u         = static_cast<T>(out_min_);
                saturated = true;
            }
            if (not saturated)
            {
                integral_ = next_integral;
            }
            out_ = u;
        }

    private:
        T      out_{};
        T      integral_{};
        T      prev_measured_{};
        T      d_filtered_{};
        double dt_member_{0.001};
        double out_min_{-1e30};
        double out_max_{1e30};
        bool   first_tick_{true};
    };
}

#endif
