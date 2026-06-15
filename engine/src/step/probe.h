#ifndef PIPER_ENGINE_STEPS_PROBE_STEP_H
#define PIPER_ENGINE_STEPS_PROBE_STEP_H

#include <string>

#include "piper/engine/step.h"

namespace piper::engine::step
{
    // Inspection sink: latches the wired value each tick so the host
    // can read it via last(). Unwired probes tick as a no-op.
    template<typename T>
    class Probe final : public Step
    {
    public:
        static std::string type_name() { return std::string("probe") + type_suffix<T>(); }

        void declare_io() override
        {
            declare_input<T>("in", InputPolicy::Optional);
        }

        void compute(Stage) override
        {
            if (has_input("in"))
            {
                last_ = input<T>("in");
            }
        }

        T const& last() const { return last_; }

    private:
        T last_{};
    };
}

#endif
