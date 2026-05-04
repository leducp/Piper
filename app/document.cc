#include "piper/app/document.h"

namespace piper::studio
{
    Document::Document(piper::Theme const&        theme,
                       piper::NodeRegistry const& registry)
        : adapter(graph, registry, theme)
        , editor(adapter)
    {
    }
}
