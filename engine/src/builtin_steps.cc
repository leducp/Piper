#include <memory>

#include "piper/engine/builtin_steps.h"

#include "piper/engine/external_io.h"
#include "piper/engine/registry.h"
#include "piper/engine/step.h"

#include "step/add.h"
#include "step/cast.h"
#include "step/constant.h"
#include "step/low_pass.h"
#include "step/random.h"
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
        register_step<step::Add<float>>     (sr);
        register_step<step::Add<double>>    (sr);
        register_step<step::Add<int32_t>>   (sr);
        register_step<step::Random>         (sr);
        register_step<step::Cast<float, int32_t>>(sr);
        register_step<step::Cast<int32_t, float>>(sr);
        register_step<step::Input<float>>   (sr);
        register_step<step::Input<int32_t>> (sr);
        register_step<step::Output<float>>  (sr);
        register_step<step::Output<int32_t>>(sr);
    }
}
