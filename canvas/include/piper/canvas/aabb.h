#ifndef PIPER_CANVAS_AABB_H
#define PIPER_CANVAS_AABB_H

#include <imgui.h>

namespace piper::canvas
{
    struct Aabb
    {
        ImVec2 min;
        ImVec2 max;

        bool intersects(Aabb const& other) const
        {
            if (max.x < other.min.x or min.x > other.max.x)
            {
                return false;
            }
            if (max.y < other.min.y or min.y > other.max.y)
            {
                return false;
            }
            return true;
        }

        bool contains(ImVec2 point) const
        {
            return point.x >= min.x and point.x <= max.x
               and point.y >= min.y and point.y <= max.y;
        }
    };
}

#endif
