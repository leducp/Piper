#include "piper/canvas/log.h"

namespace piper::canvas
{
    namespace
    {
        LogSink& sink_storage()
        {
            static LogSink instance;
            return instance;
        }
    }

    void set_log_sink(LogSink const& sink)
    {
        sink_storage() = sink;
    }

    // For framework-internal use (PR 2.6+).
    void log(LogLevel level, std::string_view message)
    {
        auto const& sink = sink_storage();
        if (sink)
        {
            sink(level, message);
        }
    }
}
