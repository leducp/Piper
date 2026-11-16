#ifndef PIPER_ENGINE_STEPS_PRESET3_STEP_H
#define PIPER_ENGINE_STEPS_PRESET3_STEP_H

#include <string>

#include "piper/engine/hashed_string.h"
#include "piper/engine/step.h"

#include "step/constant.h"  // parse_member_to<T>

namespace piper::engine::step
{
    // Three-slot mode-keyed value bank. Each slot has a value and a
    // mode string; on every tick the step publishes whichever slot's
    // mode matches the active profile's per-node mode for this
    // node. Match is by FNV-1a hash (HashedString handle) -- pre-computed in
    // declare_io(), so the hot path is three uint64 compares. If no
    // slot matches, the output is default-constructed T{}.
    template<typename T>
    class Preset3 final : public Step
    {
    public:
        static std::string type_name() { return std::string("preset3") + type_suffix<T>(); }

        void declare_io() override
        {
            value0_ = parse_member_to<T>(member("value0"));
            value1_ = parse_member_to<T>(member("value1"));
            value2_ = parse_member_to<T>(member("value2"));

            mode0_str_ = member("mode0");
            mode1_str_ = member("mode1");
            mode2_str_ = member("mode2");
            mode0_     = HashedString{mode0_str_};
            mode1_     = HashedString{mode1_str_};
            mode2_     = HashedString{mode2_str_};

            declare_output<T>("out", out_);
        }

        void compute(HashedString) override
        {
            HashedString const mode = current_mode();
            if (mode == mode0_)
            {
                out_ = value0_;
            }
            else if (mode == mode1_)
            {
                out_ = value1_;
            }
            else if (mode == mode2_)
            {
                out_ = value2_;
            }
            else
            {
                out_ = T{};
            }
        }

    private:
        T value0_{};
        T value1_{};
        T value2_{};
        T out_{};

        std::string mode0_str_;
        std::string mode1_str_;
        std::string mode2_str_;
        HashedString mode0_{};
        HashedString mode1_{};
        HashedString mode2_{};
    };
}

#endif
