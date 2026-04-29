#ifndef PIPER_ENGINE_PROBES_H
#define PIPER_ENGINE_PROBES_H

#include "piper/engine/step.h"

namespace piper::engine
{
    class ProbeFloatStep final : public Step
    {
    public:
        void declare_io() override
        {
            declare_input<float>("in");
        }

        void compute(Stage) override
        {
            last_ = input<float>("in");
        }

        float last() const { return last_; }

    private:
        float last_{0.0f};
    };

    class ProbeIntStep final : public Step
    {
    public:
        void declare_io() override
        {
            declare_input<int>("in");
        }

        void compute(Stage) override
        {
            last_ = input<int>("in");
        }

        int last() const { return last_; }

    private:
        int last_{0};
    };
}

#endif
