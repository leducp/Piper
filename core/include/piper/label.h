#ifndef PIPER_LABEL_H
#define PIPER_LABEL_H

#include <string>

#include "piper/node.h"  // NodeId, Point

namespace piper
{
    // LabelId shares the NodeId space so PinRef and Link can address
    // labels with no schema change. Graph's id counter hands out
    // unique values across nodes and labels.
    using LabelId = NodeId;
    constexpr LabelId invalid_label_id = invalid_node_id;

    enum class LabelKind
    {
        In,    // publishes the wired upstream value under `name`
        Out,   // emits the value of the matching label_in to its consumers
    };

    struct Label
    {
        LabelId     id{invalid_label_id};
        LabelKind   kind{LabelKind::In};
        std::string name;
        Point       pos;
    };

    // Pin name used by canvas/serializer/engine for the single pin
    // of each label. `LabelKind::In` exposes it as an input pin
    // (consumers wire INTO it); `LabelKind::Out` as an output pin.
    constexpr char const* label_pin_name = "pin";
}

#endif
