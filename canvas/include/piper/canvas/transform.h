#ifndef PIPER_CANVAS_TRANSFORM_H
#define PIPER_CANVAS_TRANSFORM_H

#include <imgui.h>

namespace piper::canvas
{
    // pan: canvas-space coordinate visible at the top-left of the
    //      drawable rect.
    // zoom: screen pixels per canvas unit. 1.0 = identity.
    struct Transform
    {
        ImVec2 pan{0.0f, 0.0f};
        float  zoom{1.0f};

        ImVec2 to_screen(ImVec2 canvas, ImVec2 origin) const
        {
            return ImVec2{
                origin.x + (canvas.x - pan.x) * zoom,
                origin.y + (canvas.y - pan.y) * zoom,
            };
        }

        ImVec2 to_canvas(ImVec2 screen, ImVec2 origin) const
        {
            return ImVec2{
                (screen.x - origin.x) / zoom + pan.x,
                (screen.y - origin.y) / zoom + pan.y,
            };
        }
    };
}

#endif
