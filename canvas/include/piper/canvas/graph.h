#ifndef PIPER_CANVAS_GRAPH_H
#define PIPER_CANVAS_GRAPH_H

#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>

#include "piper/canvas/ids.h"

namespace piper::canvas
{
    enum class PinKind
    {
        Input,
        Output,
    };

    enum class Connect
    {
        Allow,
        TypeMismatch,
        AlreadyConnected,    // default can_connect does NOT check this — host must.
        SameNode,            // default can_connect does NOT check this — host must.
        KindMismatch,
    };

    // Non-owning render descriptors. string_views and spans must remain
    // valid for the duration of one Editor::draw() call. Framework may
    // call nodes()/links() multiple times within a single draw(); host
    // must not mutate the underlying storage between those calls.
    // type_tag is host-defined: canvas owns no string-to-tag mapping;
    // the host is responsible for assigning and interpreting tags.
    struct Pin
    {
        PinId            id;
        PinKind          kind;
        std::string_view label;
        ImU32            color;
        uint32_t         type_tag;
    };

    struct Node
    {
        NodeId               id;
        std::string_view     title;
        ImVec2               pos;
        ImU32                header_color;
        // Final body alpha = (body_color's A channel / 255) * body_alpha.
        // body_color stays opaque in typical themes; body_alpha is the
        // per-frame mode-overlay knob.
        ImU32                body_color;
        float                body_alpha{1.0f};
        // body_min_size.x: minimum body width in canvas units.
        // body_min_size.y: extra content height *added below* the
        //   pin rows. Use it to reserve space for fields drawn in
        //   BodyRenderer. Zero means "no extra content"; the body
        //   then sizes to fit pin rows alone (with min_body_height
        //   as a floor).
        ImVec2               body_min_size{0.0f, 0.0f};
        std::span<Pin const> inputs;
        std::span<Pin const> outputs;
    };

    struct Link
    {
        LinkId  id;
        PinId   from;
        PinId   to;
        ImU32   color;
    };

    class Graph
    {
    public:
        virtual ~Graph() = default;

        virtual std::span<Node const> nodes() const = 0;
        virtual std::span<Link const> links() const = 0;

        // Default policy: type_tag equality. Host overrides to plug in
        // richer rules (e.g. piper::TypeCheck). The default DOES NOT
        // guard against same-node or already-connected — the host's
        // override is responsible for those.
        virtual Connect can_connect(Pin const& a, Pin const& b) const
        {
            if (a.type_tag != b.type_tag)
            {
                return Connect::TypeMismatch;
            }
            return Connect::Allow;
        }
    };
}

#endif
