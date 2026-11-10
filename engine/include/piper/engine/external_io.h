#ifndef PIPER_ENGINE_EXTERNAL_IO_H
#define PIPER_ENGINE_EXTERNAL_IO_H

#include "piper/engine/step.h"

namespace piper::engine::step
{
    // The "value" external input and "out" pin share storage.
    // Host writes remain available to downstream steps until changed.
    // Writes from another thread require synchronization with pipeline reads.
    template<typename T>
    class Input final : public Step
    {
    public:
        static std::string type_name() { return std::string("external_input") + type_suffix<T>(); }

        void declare_io() override
        {
            declare_external_input<T>("value", out_);
            declare_output<T>("out", out_);
        }
        void compute(Stage) override {}

    private:
        T out_{};
    };

    // Copy the linked input into storage the host can read between ticks.
    template<typename T>
    class Output final : public Step
    {
    public:
        static std::string type_name() { return std::string("external_output") + type_suffix<T>(); }

        void declare_io() override
        {
            declare_input<T>("in");
            declare_external_output<T>("value", last_);
        }
        void compute(Stage) override { last_ = input<T>("in"); }

    private:
        T last_{};
    };
}

#endif
