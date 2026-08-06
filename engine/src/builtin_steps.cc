#include <memory>
#include <type_traits>

#include "piper/engine/builtin_steps.h"

#include "piper/engine/external_io.h"
#include "piper/engine/registry.h"
#include "piper/engine/step.h"

#include "piper/builtin_types.h"
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

    // Mirrors register_scalar_nodes in core/src/builtin_nodes.cc --
    // same families, same signed/floating guards. RegistryParity in
    // tests/engine holds the two sides together.
    template<typename T>
    void register_scalar_steps(StepRegistry& sr)
    {
        register_step<step::Constant<T>>(sr);
        register_step<step::Add<T>>     (sr);
        register_step<step::Subtract<T>>(sr);
        register_step<step::Multiply<T>>(sr);
        if constexpr (std::is_signed_v<T>)
        {
            register_step<step::Abs<T>>(sr);
        }
        if constexpr (std::is_floating_point_v<T>)
        {
            register_step<step::SinWave<T>>(sr);
            register_step<step::LowPass<T>>(sr);
            register_step<step::Pid<T>>    (sr);
        }

        register_step<step::Mux3<T>>   (sr);
        register_step<step::Clamp<T>>  (sr);
        register_step<step::Preset3<T>>(sr);

        register_step<step::Input<T>> (sr);
        register_step<step::Output<T>>(sr);
    }

    template<typename... Ts>
    void register_scalar_steps_for(StepRegistry& sr, piper::TypeList<Ts...>)
    {
        (register_scalar_steps<Ts>(sr), ...);
    }

    template<typename T>
    void register_vector_steps(StepRegistry& sr)
    {
        register_step<step::Constant<T>>(sr);
        register_step<step::Add<T>>     (sr);
        register_step<step::Subtract<T>>(sr);
    }

    template<typename... Ts>
    void register_vector_steps_for(StepRegistry& sr, piper::TypeList<Ts...>)
    {
        (register_vector_steps<Ts>(sr), ...);
    }

    template<typename From, typename To>
    void register_cast_step(StepRegistry& sr)
    {
        if constexpr (not std::is_same_v<From, To>)
        {
            register_step<step::Cast<From, To>>(sr);
        }
    }

    template<typename From, typename... Ts>
    void register_casts_from(StepRegistry& sr, piper::TypeList<Ts...>)
    {
        (register_cast_step<From, Ts>(sr), ...);
    }

    template<typename... Ts>
    void register_cast_steps_for(StepRegistry& sr, piper::TypeList<Ts...> list)
    {
        (register_casts_from<Ts>(sr, list), ...);
    }

    void register_builtin_steps(StepRegistry& sr)
    {
        register_scalar_steps_for(sr, piper::BuiltinScalars{});
        register_vector_steps_for(sr, piper::BuiltinVectors{});
        register_cast_steps_for  (sr, piper::BuiltinScalars{});

        register_step<step::Random>(sr);
    }
}
