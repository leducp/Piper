#ifndef PIPER_ENGINE_STEPS_CAST_STEP_H
#define PIPER_ENGINE_STEPS_CAST_STEP_H

#include <string>

#include "piper/engine/step.h"

namespace piper::engine::step
{
    template<typename From, typename To>
    class Cast final : public Step
    {
    public:
        static std::string type_name() { return std::string("cast") + type_suffix<To>(); }

        void declare_io() override
        {
            declare_input<From>("in");
            declare_output<To>("out", out_);
        }

        void compute(Stage) override
        {
            out_ = static_cast<To>(input<From>("in"));
        }

    private:
        To out_{};
    };
}

#endif
