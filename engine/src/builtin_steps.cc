#include <memory>

#include "piper/engine/builtin_steps.h"

#include "piper/engine/external_io.h"
#include "piper/engine/registry.h"
#include "piper/engine/step.h"

#include "piper/vec.h"

#include "step/abs.h"
#include "step/add.h"
#include "step/cast.h"
#include "step/clamp.h"
#include "step/constant.h"
#include "step/low_pass.h"
#include "step/multiply.h"
#include "step/mux3.h"
#include "step/pid.h"
#include "step/preset3.h"
#include "step/random.h"
#include "step/sin_wave.h"
#include "step/subtract.h"

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
        register_step<step::Constant<piper::Vec2<float>>>(sr);
        register_step<step::Constant<piper::Vec3<float>>>(sr);
        register_step<step::SinWave<float>> (sr);
        register_step<step::SinWave<double>>(sr);
        register_step<step::LowPass<float>> (sr);
        register_step<step::LowPass<double>>(sr);
        register_step<step::Add<float>>      (sr);
        register_step<step::Add<double>>     (sr);
        register_step<step::Add<int32_t>>    (sr);
        register_step<step::Subtract<float>> (sr);
        register_step<step::Subtract<double>>(sr);
        register_step<step::Subtract<int32_t>>(sr);
        register_step<step::Add<piper::Vec2<float>>>     (sr);
        register_step<step::Add<piper::Vec3<float>>>     (sr);
        register_step<step::Subtract<piper::Vec2<float>>>(sr);
        register_step<step::Subtract<piper::Vec3<float>>>(sr);
        register_step<step::Multiply<float>>  (sr);
        register_step<step::Multiply<double>> (sr);
        register_step<step::Multiply<int32_t>>(sr);
        register_step<step::Abs<float>>     (sr);
        register_step<step::Abs<double>>    (sr);
        register_step<step::Abs<int32_t>>   (sr);
        register_step<step::Mux3<float>>    (sr);
        register_step<step::Mux3<double>>   (sr);
        register_step<step::Mux3<int32_t>>  (sr);
        register_step<step::Clamp<float>>   (sr);
        register_step<step::Clamp<double>>  (sr);
        register_step<step::Clamp<int32_t>> (sr);
        register_step<step::Pid<float>>     (sr);
        register_step<step::Pid<double>>    (sr);
        register_step<step::Preset3<float>>   (sr);
        register_step<step::Preset3<double>>  (sr);
        register_step<step::Preset3<int32_t>> (sr);
        register_step<step::Random>         (sr);
        register_step<step::Cast<float, int32_t>>(sr);
        register_step<step::Cast<int32_t, float>>(sr);
        register_step<step::Input<float>>   (sr);
        register_step<step::Input<double>>  (sr);
        register_step<step::Input<int32_t>> (sr);
        register_step<step::Output<float>>  (sr);
        register_step<step::Output<double>> (sr);
        register_step<step::Output<int32_t>>(sr);
    }
}
