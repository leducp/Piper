#include "piper/engine/builtin_steps.h"

#include <memory>

#include "piper/engine/external_io.h"
#include "piper/engine/registry.h"
#include "piper/engine/step.h"

#include "step/constant.h"
#include "step/low_pass.h"
#include "step/sin_wave.h"

namespace piper::engine
{
    template<typename StepT>
    void register_step(StepRegistry& sr)
    {
        sr.add(StepT::type_name(), []
        {
            return std::make_shared<StepT>();
        });
    }

    void register_builtin_steps(StepRegistry& sr)
    {
        register_step<step::Constant<float>>(sr);
        register_step<step::Constant<int32_t>>  (sr);
        register_step<step::SinWave<float>> (sr);
        register_step<step::SinWave<double>>(sr);
        register_step<step::LowPass<float>> (sr);
        register_step<step::LowPass<double>>(sr);
        register_step<step::Input<float>>   (sr);
        register_step<step::Input<int32_t>>     (sr);
        register_step<step::Output<float>>  (sr);
        register_step<step::Output<int32_t>>    (sr);
    }
}
