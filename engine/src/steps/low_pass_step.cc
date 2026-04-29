#include <memory>
#include <numbers>
#include <string>

#include "piper/engine/registry.h"
#include "piper/engine/step.h"

namespace piper::engine
{
    class LowPassStep final : public Step
    {
    public:
        void declare_io() override
        {
            declare_input<float>("in");
            publish_output<float>("out", out_);
            cutoff_ = std::stod(member("cutoff"));
        }

        void compute(Stage) override
        {
            float const in    = input<float>("in");
            double const tau   = 1.0 / (2.0 * std::numbers::pi_v<double> * cutoff_);
            double const alpha = tick_period / (tau + tick_period);
            double const next  = static_cast<double>(out_) + alpha * (static_cast<double>(in) - static_cast<double>(out_));
            out_ = static_cast<float>(next);
        }

    private:
        static constexpr double tick_period = 0.001;

        float  out_{0.0f};
        double cutoff_{10.0};
    };

    void register_low_pass_step(StepRegistry& sr)
    {
        sr.add("low_pass", []
        {
            return std::unique_ptr<Step>{ new LowPassStep{} };
        });
    }
}
