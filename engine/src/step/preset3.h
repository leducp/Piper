#ifndef PIPER_ENGINE_STEPS_PRESET3_STEP_H
#define PIPER_ENGINE_STEPS_PRESET3_STEP_H

#include <string>

#include "piper/engine/mode.h"
#include "piper/engine/step.h"

#include "step/constant.h"  // parse_member_to<T>

namespace piper::engine::step
{
    // Three-slot mode-keyed value bank. Each slot has a value and a
    // label string; on every tick the step publishes whichever slot's
    // label matches the active mode profile's per-node label for this
    // node. Match is by FNV-1a hash (Mode handle) -- pre-computed in
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

            label0_str_ = member("label0");
            label1_str_ = member("label1");
            label2_str_ = member("label2");
            label0_     = Mode{label0_str_};
            label1_     = Mode{label1_str_};
            label2_     = Mode{label2_str_};

            declare_output<T>("out", out_);
        }

        void compute(Stage) override
        {
            Mode const lbl = current_label();
            if (lbl == label0_)
            {
                out_ = value0_;
            }
            else if (lbl == label1_)
            {
                out_ = value1_;
            }
            else if (lbl == label2_)
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

        std::string label0_str_;
        std::string label1_str_;
        std::string label2_str_;
        Mode        label0_{};
        Mode        label1_{};
        Mode        label2_{};
    };
}

#endif
