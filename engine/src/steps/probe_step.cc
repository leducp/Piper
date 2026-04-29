#include "piper/engine/probes.h"

#include <memory>

#include "piper/engine/registry.h"
#include "piper/engine/step.h"

namespace piper::engine
{
    void register_probe_float_step(StepRegistry& sr)
    {
        sr.add("probe<float>", []
        {
            return std::unique_ptr<Step>{ new ProbeFloatStep{} };
        });
    }

    void register_probe_int_step(StepRegistry& sr)
    {
        sr.add("probe<int>", []
        {
            return std::unique_ptr<Step>{ new ProbeIntStep{} };
        });
    }
}
