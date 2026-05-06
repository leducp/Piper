#ifndef PIPER_ENGINE_STEPS_MUX3_STEP_H
#define PIPER_ENGINE_STEPS_MUX3_STEP_H

#include <cstdint>
#include <string>

#include "piper/engine/step.h"

namespace piper::engine::step
{
    // Three-input typed multiplexer with an int32_t selector. The
    // selector saturates at 0 and 2 -- out-of-range values pick in0
    // or in2 rather than producing undefined output.
    template<typename T>
    class Mux3 final : public Step
    {
    public:
        static std::string type_name() { return std::string("mux3") + type_suffix<T>(); }

        void declare_io() override
        {
            declare_input<T>("in0");
            declare_input<T>("in1");
            declare_input<T>("in2");
            declare_input<int32_t>("select");
            declare_output<T>("out", out_);
        }

        void compute(Stage) override
        {
            int32_t const sel = input<int32_t>("select");
            if (sel <= 0)
            {
                out_ = input<T>("in0");
            }
            else if (sel == 1)
            {
                out_ = input<T>("in1");
            }
            else
            {
                out_ = input<T>("in2");
            }
        }

    private:
        T out_{};
    };
}

#endif
