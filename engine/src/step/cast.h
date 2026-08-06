#ifndef PIPER_ENGINE_STEPS_CAST_STEP_H
#define PIPER_ENGINE_STEPS_CAST_STEP_H

#include <string>

#include "piper/engine/step.h"

namespace piper::engine::step
{
    // Named by both ends: the destination alone does not identify the
    // conversion once more than two scalar types exist.
    template<typename From, typename To>
    class Cast final : public Step
    {
    public:
        static std::string type_name()
        {
            return std::string("cast<") + piper::data_type_string<From>()
                 + "," + piper::data_type_string<To>() + ">";
        }

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
