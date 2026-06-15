#include <stdexcept>
#include <string>

#include "piper/engine/step.h"

namespace piper::engine
{
    Step::~Step() = default;

    Step::ExternalIoKind Step::external_io_kind() const
    {
        return ExternalIoKind::None;
    }

    std::string_view Step::external_io_type() const
    {
        return {};
    }

    void Step::require_io() const
    {
        if (io_ == nullptr)
        {
            throw std::logic_error("Step I/O not initialized; only valid inside declare_io()/compute()");
        }
    }

    std::string const& Step::member(std::string_view name) const
    {
        require_io();
        auto it = io_->members.find(std::string(name));
        if (it == io_->members.end())
        {
            throw std::out_of_range("Step::member: unknown member '" + std::string(name) + "'");
        }
        return it->second;
    }

    Mode Step::current_mode() const
    {
        require_io();
        if (io_->current_mode == nullptr)
        {
            return {};
        }
        return *io_->current_mode;
    }

    Mode Step::current_label() const
    {
        require_io();
        return io_->current_label;
    }

    OutputSlot& Step::output_slot(std::string_view name)
    {
        require_io();
        auto it = io_->output_slots.find(std::string(name));
        if (it == io_->output_slots.end())
        {
            throw std::out_of_range("Step::output: unknown output '" + std::string(name) + "'");
        }
        return it->second;
    }

    OutputSlot const& Step::output_slot(std::string_view name) const
    {
        require_io();
        auto it = io_->output_slots.find(std::string(name));
        if (it == io_->output_slots.end())
        {
            throw std::out_of_range("Step::output: unknown output '" + std::string(name) + "'");
        }
        return it->second;
    }
}
