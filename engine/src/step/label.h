#ifndef PIPER_ENGINE_STEPS_LABEL_STEP_H
#define PIPER_ENGINE_STEPS_LABEL_STEP_H

#include <string>

#include "piper/engine/step.h"

namespace piper::engine::step
{
    // label_in / label_out are wiring affordances, not computations.
    // Engine::build resolves them into synthetic links between their
    // upstream producer and downstream consumer pins; the steps below
    // are placeholders so the StepRegistry has a factory.

    class LabelIn final : public Step
    {
    public:
        static std::string type_name() { return "label_in"; }

        void declare_io() override {}
        void compute(Stage) override {}
    };

    class LabelOut final : public Step
    {
    public:
        static std::string type_name() { return "label_out"; }

        void declare_io() override {}
        void compute(Stage) override {}
    };
}

#endif
