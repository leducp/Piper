#include <cmath>
#include <memory>
#include <numbers>
#include <string>

#include "piper/engine/registry.h"
#include "piper/engine/step.h"

namespace piper::engine
{
    class SinWaveStep final : public Step
    {
    public:
        void declare_io() override
        {
            publish_output<float>("out", out_);
            frequency_ = std::stod(member("frequency"));
            amplitude_ = std::stod(member("amplitude"));
            phase_     = std::stod(member("phase"));
        }

        void compute(Stage) override
        {
            double const angle = 2.0 * std::numbers::pi_v<double> * frequency_ * t_ + phase_;
            out_ = static_cast<float>(amplitude_ * std::sin(angle));
            t_ += tick_period;
        }

    private:
        static constexpr double tick_period = 0.001;

        float  out_{0.0f};
        double t_{0.0};
        double frequency_{1.0};
        double amplitude_{1.0};
        double phase_{0.0};
    };

    void register_sin_wave_step(StepRegistry& sr)
    {
        sr.add("sin_wave", []
        {
            return std::unique_ptr<Step>{ new SinWaveStep{} };
        });
    }
}
