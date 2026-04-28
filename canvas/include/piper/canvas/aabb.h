#ifndef PIPER_CANVAS_AABB_H
#define PIPER_CANVAS_AABB_H

#include <imgui.h>

namespace piper::canvas
{
    // Axis-Aligned Bounding Box: a rectangle whose edges are parallel
    // to the X and Y axes. Described by just two corners (no rotation).
    // Cheap intersection / point-in-box checks make it the workhorse
    // for cull and hit-test.
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

        bool contains(ImVec2 const& point) const
        {
            return point.x >= min.x and point.x <= max.x
               and point.y >= min.y and point.y <= max.y;
        }
    };
}

#endif
