#include <memory>
#include <string>

#include "piper/engine/registry.h"
#include "piper/engine/step.h"

namespace piper::engine
{
    class ConstantFloatStep final : public Step
    {
    public:
        void declare_io() override
        {
            publish_output<float>("out", out_);
            out_ = std::stof(member("value"));
        }

        // value is captured at declare_io; nothing to do per tick.
        void compute(Stage) override {}

    private:
        float out_{0.0f};
    };

    class ConstantIntStep final : public Step
    {
    public:
        void declare_io() override
        {
            publish_output<int>("out", out_);
            out_ = std::stoi(member("value"));
        }

        void compute(Stage) override {}

    private:
        int out_{0};
    };

    void register_constant_float_step(StepRegistry& sr)
    {
        sr.add("constant<float>", []
        {
            return std::unique_ptr<Step>{ new ConstantFloatStep{} };
        });
    }

    void register_constant_int_step(StepRegistry& sr)
    {
        sr.add("constant<int>", []
        {
            return std::unique_ptr<Step>{ new ConstantIntStep{} };
        });
    }
}
