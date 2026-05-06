#ifndef PIPER_ENGINE_STEP_H
#define PIPER_ENGINE_STEP_H

#include <any>
#include <cstddef>
#include <stdint.h>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "piper/node.h"

#include "piper/engine/mode.h"
#include "piper/engine/stage.h"

namespace piper::engine
{
    class Engine;
    class Step;

    // type_tag<T>::suffix produces the canonical "<T>" suffix used to
    // compose Step type strings ("constant<float>", "low_pass<double>",
    // ...). Adding a new built-in T is one line:
    //     PIPER_ENGINE_DECLARE_TYPE_TAG(my_t);
    // Using type_suffix<T>() with an unregistered T is a compile error.
    template<typename T> struct type_tag;

#define PIPER_ENGINE_DECLARE_TYPE_TAG(T)                            \
    template<> struct type_tag<T>                                   \
    {                                                               \
        static constexpr char const* suffix = "<" #T ">";           \
    }

    PIPER_ENGINE_DECLARE_TYPE_TAG(float);
    PIPER_ENGINE_DECLARE_TYPE_TAG(double);
    PIPER_ENGINE_DECLARE_TYPE_TAG(int8_t);
    PIPER_ENGINE_DECLARE_TYPE_TAG(int16_t);
    PIPER_ENGINE_DECLARE_TYPE_TAG(int32_t);
    PIPER_ENGINE_DECLARE_TYPE_TAG(int64_t);
    PIPER_ENGINE_DECLARE_TYPE_TAG(uint8_t);
    PIPER_ENGINE_DECLARE_TYPE_TAG(uint16_t);
    PIPER_ENGINE_DECLARE_TYPE_TAG(uint32_t);
    PIPER_ENGINE_DECLARE_TYPE_TAG(uint64_t);

#undef PIPER_ENGINE_DECLARE_TYPE_TAG

    template<typename T>
    consteval char const* type_suffix() { return type_tag<T>::suffix; }

    struct OutputSlot
    {
        void*    data{nullptr};
        std::any ref_any;     // reference_wrapper<T const>(*data)
    };

    // Throws std::runtime_error if the slot was published with a type
    // other than T. Used before reinterpreting slot.data as T*.
    template<typename T>
    void check_output_type(OutputSlot const& slot, std::string_view name)
    {
        if (std::any_cast<std::reference_wrapper<T const>>(&slot.ref_any) == nullptr)
        {
            throw std::runtime_error("Step::output: type mismatch for '" + std::string(name) + "'");
        }
    }

    // The matcher returns true iff the producer's `ref_any` was
    // published with the same T this declaration expects. It is a
    // function pointer (one per T), instantiated by declare_input<T>.
    // No RTTI required: the std::any_cast pointer-form below uses
    // std::any's manager pointer for identity, not typeid.
    struct InputSlot
    {
        bool (*matches)(std::any const&){nullptr};
    };

    // Per-step runtime block. Step::io points at this; Engine owns it.
    // unordered_map references stay valid across rehash, so producer
    // outputs reached via the wired ref_any are address-stable.
    struct IoBlock
    {
        piper::NodeId                                 node_id{piper::invalid_node_id};
        std::shared_ptr<Step>                         step;
        std::unordered_map<std::string, OutputSlot>   output_slots;
        std::unordered_map<std::string, InputSlot>    input_slots;
        std::unordered_map<std::string, std::any>     inputs;     // any: reference_wrapper<T const>
        std::unordered_map<std::string, std::string>  members;
        std::vector<uint16_t>                         active_stage_indices;
        // Active profile handle on the owning Engine. Address is
        // stable across set_mode() calls; null until build() wires it.
        Mode const*                                   current_mode{nullptr};
        // This node's label in the active profile (per-node entry of
        // ModeProfile::per_node) bundled with its FNV-1a hash. Engine
        // rewrites both fields on set_mode; the Mode's string_view
        // points into label_buf below.
        std::string                                   label_buf;
        Mode                                          current_label{};
    };

    class Step
    {
    public:
        virtual ~Step();

        virtual void compute(Stage current) = 0;

        // io is null until just before declare_io() is invoked by
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

        // Active profile handle. `.name` is empty when neither
        // set_mode nor a default_mode has fired. Compares are O(1)
        // hash compares: `current_mode() == "loose"` is just one
        // uint64 compare at -O1+.
        Mode current_mode() const;

        // This node's label in the active profile. Free-form and
        // step-defined ("loose", "passthrough", ...); "disable" is
        // the one reserved value the engine itself acts on (skips
        // compute() entirely, so a step never observes it). `.name`
        // is empty when the active profile has no entry for this
        // node, or the mode is unknown / unset.
        Mode current_label() const;

        // Inside declare_io(): declare the C++ type expected by an
        // input pin. Engine checks it against the upstream producer's
        // published type at link wire time via std::any_cast on the
        // producer's ref_any.
        template<typename T>
        void declare_input(std::string_view name)
        {
            io_->input_slots[std::string(name)] = InputSlot{
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
            io_->output_slots[std::string(name)] = std::move(s);
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

        // Engine calls this exactly once per Step instance, just
        // before declare_io(). A second call -- or any user-code call
        // -- throws std::logic_error.
        void init(IoBlock& block)
        {
            if (io_ != nullptr)
            {
                throw std::logic_error("Step::init: already initialized");
            }
            io_ = &block;
        }

    private:
        IoBlock*                                  io_{nullptr};
        std::unordered_map<std::string, std::any> managed_outputs_;

        OutputSlot&       output_slot(std::string_view name);
        OutputSlot const& output_slot(std::string_view name) const;
    };
}

#endif
