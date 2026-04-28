#ifndef PIPER_CANVAS_LOG_H
#define PIPER_CANVAS_LOG_H

#include <functional>
#include <string_view>

namespace piper::canvas
{
    enum class LogLevel
    {
        Debug,
        Info,
        Warning,
        Error,
    };

    using LogSink = std::function<void(LogLevel, std::string_view)>;

    // Default sink is no-op; framework never writes to stderr directly.
    void set_log_sink(LogSink const& sink);
}

#endif
