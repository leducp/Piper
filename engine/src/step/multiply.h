#ifndef PIPER_ENGINE_STEPS_MULTIPLY_STEP_H
#define PIPER_ENGINE_STEPS_MULTIPLY_STEP_H

#include <string>

#include "piper/engine/step.h"

namespace piper::engine::step
{
    template<typename T>
    class Multiply final : public Step
    {
    public:
        static std::string type_name() { return std::string("multiply") + type_suffix<T>(); }

        void declare_io() override
        {
            declare_input<T>("a");
            declare_input<T>("b");
            declare_output<T>("out", out_);
        }

        void compute(Stage) override
        {
            out_ = input<T>("a") * input<T>("b");
        }

    private:
        T out_{};
    };
}

#endif
