#include "piper/engine/step.h"

#include <stdexcept>
#include <string>

namespace piper::engine
{
    Step::~Step() = default;

    std::string const& Step::member(std::string_view name) const
    {
        auto it = io_->members.find(std::string(name));
        if (it == io_->members.end())
        {
            throw std::out_of_range("Step::member: unknown member '" + std::string(name) + "'");
        }
        return it->second;
    }

    OutputSlot& Step::output_slot(std::string_view name)
    {
        auto it = io_->output_slots.find(std::string(name));
        if (it == io_->output_slots.end())
        {
            throw std::out_of_range("Step::output: unknown output '" + std::string(name) + "'");
        }
        return it->second;
    }

    OutputSlot const& Step::output_slot(std::string_view name) const
    {
        auto it = io_->output_slots.find(std::string(name));
        if (it == io_->output_slots.end())
        {
            throw std::out_of_range("Step::output: unknown output '" + std::string(name) + "'");
        }
        return it->second;
    }
}
