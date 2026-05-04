#ifndef PIPER_ANNOTATION_H
#define PIPER_ANNOTATION_H

#include <stdint.h>
#include <string>

#include "piper/color.h"
#include "piper/node.h"

namespace piper
{
    using AnnotationId = uint64_t;
    constexpr AnnotationId invalid_annotation_id = 0;

    struct Annotation
    {
        AnnotationId id{invalid_annotation_id};
        Point        pos;
        Point        size{200.0f, 100.0f};
        std::string  text;
        rgba         color{rgba::from_components(0xFF, 0xC0, 0x40, 0x40)};
    };
}

#endif
