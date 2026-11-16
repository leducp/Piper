#include <stdexcept>
#include <string>

#include "piper/engine/step.h"

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

    HashedString Step::current_profile() const
    {
        if (io_->current_profile == nullptr)
        {
            return {};
        }
        return *io_->current_profile;
    }

    HashedString Step::current_mode() const
    {
        return io_->current_mode;
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
