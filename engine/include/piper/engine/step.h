#ifndef PIPER_ENGINE_STEP_H
#define PIPER_ENGINE_STEP_H

#include <any>
#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "piper/node.h"

#include "piper/engine/stage.h"

namespace piper::engine
{
    class Engine;
    class Step;

    // Canonical "<T>" suffix used to compose Step type strings such as
    // "constant<float>" or "external_input<int>". Add a branch when a
    // new built-in T is introduced.
    template<typename T>
    constexpr char const* type_suffix()
    {
        if constexpr (std::is_same_v<T, float>)
        {
            return "<float>";
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            return "<double>";
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            return "<int>";
        }
        else
        {
            static_assert(sizeof(T) == 0, "type_suffix<T>: unsupported T");
        }
    }

    struct OutputSlot
    {
        void*    data{nullptr};
        std::any ref_any;     // reference_wrapper<T const>(*data)
    };

    // The matcher returns true iff the producer's `ref_any` was
    // published with the same T this declaration expects. It is a
    // function pointer (one per T), instantiated by declare_input<T>.
    // No RTTI required: the std::any_cast pointer-form below uses
    // std::any's manager pointer for identity, not typeid.
    struct InputDecl
    {
        bool (*matches)(std::any const&){nullptr};
    };

    // Per-step runtime block. Step::io_ points at this; Engine owns it.
    // unordered_map references stay valid across rehash, so producer
    // outputs reached via the wired ref_any are address-stable.
    struct IoBlock
    {
        piper::NodeId                                 node_id{piper::invalid_node_id};
        std::shared_ptr<Step>                         step;
        std::unordered_map<std::string, OutputSlot>   outputs;
        std::unordered_map<std::string, InputDecl>    input_decls;
        std::unordered_map<std::string, std::any>     inputs;     // any: reference_wrapper<T const>
        std::unordered_map<std::string, std::string>  members;
        std::vector<std::size_t>                      active_stage_indices;
    };

    class Step
    {
    public:
        virtual ~Step();

        virtual void compute(Stage current) = 0;

        // io_ is null until just before declare_io() is invoked by
        // Engine::build(); calling input/output/member from a Step's
        // constructor crashes.
        virtual void declare_io() {}

        // Read a wired input. Throws if the input was not wired by a
        // link or if T does not match the producer's published type.
        template<typename T>
        T const& input(std::string_view name) const
        {
            auto it = io_->inputs.find(std::string(name));
            if (it == io_->inputs.end())
            {
                throw std::out_of_range("Step::input: unwired input '" + std::string(name) + "'");
            }
            return std::any_cast<std::reference_wrapper<T const>>(it->second).get();
        }

        // Read or write a published output. Throws if the name is not a
        // declared output or if T does not match the published type.
        template<typename T>
        T& output(std::string_view name)
        {
            auto& slot = output_slot(name);
            check_output_type<T>(slot, name);
            return *static_cast<T*>(slot.data);
        }

        template<typename T>
        T const& output(std::string_view name) const
        {
            auto const& slot = output_slot(name);
            check_output_type<T>(slot, name);
            return *static_cast<T const*>(slot.data);
        }

        std::string const& member(std::string_view name) const;

        // Inside declare_io(): declare the C++ type expected by an
        // input pin. Engine checks it against the upstream producer's
        // published type at link wire time via std::any_cast on the
        // producer's ref_any.
        template<typename T>
        void declare_input(std::string_view name)
        {
            io_->input_decls[std::string(name)] = InputDecl{
                [](std::any const& a)
                {
                    return std::any_cast<std::reference_wrapper<T const>>(&a) != nullptr;
                }
            };
        }

        // Inside declare_io(): declare a typed output backed by a
        // caller-owned member at &slot. The slot must outlive this
        // Step; the Engine keeps the Step alive via its IoBlock.
        template<typename T>
        void declare_output(std::string_view name, T& slot)
        {
            OutputSlot s;
            s.data    = static_cast<void*>(&slot);
            s.ref_any = std::any{ std::cref(slot) };
            io_->outputs[std::string(name)] = std::move(s);
        }

        // Inside declare_io(): declare a typed output whose storage is
        // engine-managed (no caller-owned slot). Use this when the Step
        // does not have a fixed C++ member field for the output, such
        // as Python-authored steps and generic adapters. T must be
        // default-constructible.
        template<typename T>
        void declare_output(std::string_view name)
        {
            auto& slot = managed_outputs_[std::string(name)];
            slot.template emplace<T>();
            declare_output<T>(name, std::any_cast<T&>(slot));
        }

        // Sugar for output<T>(name) = value. Works for both
        // caller-owned and engine-managed outputs. Throws if the name
        // is not a declared output or T does not match.
        template<typename T>
        void set_output(std::string_view name, T const& value)
        {
            output<T>(name) = value;
        }

    private:
        friend class Engine;
        IoBlock* io_{nullptr};
        std::unordered_map<std::string, std::any> managed_outputs_;

        OutputSlot&       output_slot(std::string_view name);
        OutputSlot const& output_slot(std::string_view name) const;

        template<typename T>
        static void check_output_type(OutputSlot const& slot, std::string_view name)
        {
            if (std::any_cast<std::reference_wrapper<T const>>(&slot.ref_any) == nullptr)
            {
                throw std::runtime_error("Step::output: type mismatch for '" + std::string(name) + "'");
            }
        }
    };
}

#endif
