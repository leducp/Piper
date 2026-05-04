#ifndef PIPER_APP_MINIMAP_H
#define PIPER_APP_MINIMAP_H

#include "piper/app/document.h"
#include "piper/canvas/cull.h"

namespace piper::studio
{
    // Renders a graph overview in the canvas pane's bottom-right and
    // returns the canvas-space target of any in-flight click/drag pan
    // gesture. Caller is expected to call doc.editor.center_on(*target)
    // when the optional is set. No-op when the canvas pane is too
    // small for the mini-map to fit, or when the graph has no nodes.
    void draw_minimap(Document& doc,
                      canvas::LayoutMetrics const& layout,
                      float dpi_scale);
}

#endif
