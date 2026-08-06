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

#include "piper/builtin_types.h"
#include "piper/node.h"
#include "piper/vec.h"

#include "piper/engine/mode.h"
#include "piper/engine/stage.h"

namespace piper::engine
{
    class Engine;
    class Step;

    // The canonical "<T>" suffix used to compose Step type strings
    // ("constant<float>", "low_pass<double>", ...). The tag inside the
    // brackets is piper::data_type_string<T>() -- the same string the
    // node registry puts in AttributeSpec::data_type -- so a Step name
    // cannot drift from its NodeType name. Declare a custom T with
    // PIPER_DECLARE_DATA_TYPE_TAG (piper/builtin_types.h); using an
    // undeclared T here is a compile error.
    template<typename T>
    std::string type_suffix()
    {
        return std::string("<") + piper::data_type_string<T>() + ">";
    }

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
        // Optional inputs may be left unwired; the step falls back (it
        // guards reads with has_input). build() only errors on unwired
        // required inputs.
        bool optional{false};
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

        // True iff a producer is wired to the input pin `name`. Used by
        // steps that expose a member with an optional input override
        // (e.g. dt on sin_wave / low_pass / pid).
        bool has_input(std::string_view name) const
        {
            return io_->inputs.count(std::string(name)) != 0;
        }

        // Read the optional "dt_in" pin as a double, falling back to the
        // step's member timestep when unwired. Shared by the time-
        // stepped steps (sin_wave, low_pass, pid).
        template<typename T>
        double resolve_dt(double dt_member) const
        {
            if (has_input("dt_in"))
            {
                return static_cast<double>(input<T>("dt_in"));
            }
            return dt_member;
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
        void declare_input(std::string_view name, bool optional = false)
        {
            io_->input_slots[std::string(name)] = InputSlot{
                [](std::any const& a)
                {
                    return std::any_cast<std::reference_wrapper<T const>>(&a) != nullptr;
                },
                optional
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

        // Engine binds the step to its per-engine IoBlock: once at
        // build() (just before declare_io()) and again before every
        // compute() call at tick time. The per-tick rebind is what lets
        // ONE live step instance serve several engines -- e.g. a
        // hardware-singleton device step shared by two pipelines: each
        // engine wires its own IoBlock, and input()/output() resolve
        // against whichever engine is currently ticking. Engines are
        // single-threaded and tick sequentially, so this is race-free;
        // the cost is one pointer assignment per step per stage.
        void init(IoBlock& block)
        {
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
