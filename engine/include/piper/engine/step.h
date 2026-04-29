#ifndef PIPER_ENGINE_STEP_H
#define PIPER_ENGINE_STEP_H

#include <any>
#include <functional>
#include <string>
#include <string_view>
#include <typeinfo>

#include "piper/engine/stage.h"

namespace piper::engine
{
    class Engine;

    class Step
    {
    public:
        virtual ~Step() = default;

        virtual void compute(Stage current) = 0;

        // io_ is null until just before declare_io() is invoked by
        // Engine::build(); calling input/output/member from a Step's
        // constructor crashes.
        virtual void declare_io() {}

    protected:
        template <class T>
        T const& input(std::string_view name) const
        {
            std::any const& slot = input_slot(name);
            return std::any_cast<std::reference_wrapper<T const>>(slot).get();
        }

        template <class T>
        T& output(std::string_view name)
        {
            return *static_cast<T*>(output_data(name, &typeid(T)));
        }

        std::string const& member(std::string_view name) const;

        // Inside declare_io(): publish a typed output backed by a
        // step-owned member at &slot. Engine snapshots typeid(T) and a
        // reference_wrapper<T const>(slot). The slot must outlive this
        // Step; the Engine guarantees that as long as the Step is held
        // by its IoBlock.
        template <class T>
        void publish_output(std::string_view name, T& slot)
        {
            publish_output_impl(name,
                                &typeid(T),
                                static_cast<void*>(&slot),
                                std::any{ std::cref(slot) });
        }

        // Inside declare_io(): declare the C++ type expected by an
        // input pin. Engine compares it against the upstream
        // producer's published typeid at link wire time.
        template <class T>
        void declare_input(std::string_view name)
        {
            declare_input_impl(name, &typeid(T));
        }

    private:
        friend class Engine;
        struct IoBlock;
        IoBlock* io_{nullptr};

        std::any const& input_slot(std::string_view name) const;
        void*           output_data(std::string_view name,
                                    std::type_info const* expected) const;
        void            publish_output_impl(std::string_view name,
                                            std::type_info const* type,
                                            void* data,
                                            std::any ref_any);
        void            declare_input_impl(std::string_view name,
                                           std::type_info const* type);
    };
}

#endif
