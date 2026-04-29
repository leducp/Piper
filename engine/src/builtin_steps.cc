#include "piper/engine/builtin_steps.h"

namespace piper::engine
{
    void register_constant_float_step(StepRegistry& sr);
    void register_constant_int_step  (StepRegistry& sr);
    void register_sin_wave_step      (StepRegistry& sr);
    void register_low_pass_step      (StepRegistry& sr);
    void register_probe_float_step   (StepRegistry& sr);
    void register_probe_int_step     (StepRegistry& sr);

    void register_builtin_steps(StepRegistry& sr)
    {
        register_constant_float_step(sr);
        register_constant_int_step  (sr);
        register_sin_wave_step      (sr);
        register_low_pass_step      (sr);
        register_probe_float_step   (sr);
        register_probe_int_step     (sr);
    }
}
