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

    std::string_view Step::current_mode() const
    {
        if (io_->current_mode == nullptr)
        {
            return {};
        }
        return *io_->current_mode;
    }

    uint64_t Step::current_mode_id() const
    {
        if (io_->current_mode_id == nullptr)
        {
            return 0;
        }
        return *io_->current_mode_id;
    }

    std::string_view Step::current_label() const
    {
        return io_->current_label;
    }

    uint64_t Step::current_label_id() const
    {
        return io_->current_label_id;
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
