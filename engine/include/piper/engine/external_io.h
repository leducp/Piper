#ifndef PIPER_ENGINE_EXTERNAL_IO_H
#define PIPER_ENGINE_EXTERNAL_IO_H

#include "piper/engine/step.h"

namespace piper::engine::step
{
    // Externally-driven input. The host process writes its value via
    // set(); the pipeline reads it through the "out" pin like any
    // other producer. compute() does nothing -- out_ holds whatever
    // the host last wrote. Single-threaded write/read; if the host
    // updates from a separate thread, wrap T in std::atomic at call
    // sites.
    template<typename T>
    class Input final : public Step
    {
    public:
        static std::string type_name() { return std::string("external_input") + type_suffix<T>(); }

        void declare_io() override { declare_output<T>("out", out_); }
        void compute(Stage) override {}

        ExternalIoKind   external_io_kind() const override { return ExternalIoKind::Input; }
        std::string_view external_io_type() const override { return type_suffix<T>(); }

        void     set(T const& value) { out_ = value; }
        T const& get() const         { return out_; }

    private:
        T out_{};
    };

    // Externally-read output. compute() copies the wired input into
    // a cached slot so the host can read it via get() outside of any
    // tick.
    template<typename T>
    class Output final : public Step
    {
    public:
        static std::string type_name() { return std::string("external_output") + type_suffix<T>(); }

        void declare_io() override { declare_input<T>("in"); }
        void compute(Stage) override { last_ = input<T>("in"); }

        ExternalIoKind   external_io_kind() const override { return ExternalIoKind::Output; }
        std::string_view external_io_type() const override { return type_suffix<T>(); }

        T const& get() const { return last_; }

    private:
        T last_{};
    };
}

#endif
