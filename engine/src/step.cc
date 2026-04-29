#include "piper/engine/step.h"

#include <stdexcept>
#include <string>

#include "io_block.h"

namespace piper::engine
{
    std::string const& Step::member(std::string_view name) const
    {
        auto it = io_->members.find(std::string(name));
        if (it == io_->members.end())
        {
            throw std::out_of_range("Step::member: unknown member '" + std::string(name) + "'");
        }
        return it->second;
    }

    std::any const& Step::input_slot(std::string_view name) const
    {
        auto it = io_->inputs.find(std::string(name));
        if (it == io_->inputs.end())
        {
            throw std::out_of_range("Step::input: unwired input '" + std::string(name) + "'");
        }
        return it->second;
    }

    void* Step::output_data(std::string_view name, std::type_info const* expected) const
    {
        auto it = io_->outputs.find(std::string(name));
        if (it == io_->outputs.end())
        {
            throw std::out_of_range("Step::output: unknown output '" + std::string(name) + "'");
        }
        if (*it->second.type != *expected)
        {
            throw std::runtime_error("Step::output: type mismatch for '" + std::string(name) + "'");
        }
        return it->second.data;
    }

    void Step::publish_output_impl(std::string_view name,
                                   std::type_info const* type,
                                   void* data,
                                   std::any ref_any)
    {
        OutputSlot slot;
        slot.data    = data;
        slot.type    = type;
        slot.ref_any = std::move(ref_any);
        io_->outputs[std::string(name)] = std::move(slot);
    }

    void Step::declare_input_impl(std::string_view name,
                                  std::type_info const* type)
    {
        io_->input_decls[std::string(name)] = InputDecl{ type };
    }
}
