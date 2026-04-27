#include "piper/connect.h"

#include "piper/attribute.h"
#include "piper/node.h"

namespace piper
{
    Connect validate_connection(Graph const& g,
                                PinRef const& from,
                                PinRef const& to,
                                TypeCheck const& tc)
    {
        Node const* from_node = g.find_node(from.node);
        if (from_node == nullptr)
        {
            return Connect::UnknownPin;
        }
        Node const* to_node = g.find_node(to.node);
        if (to_node == nullptr)
        {
            return Connect::UnknownPin;
        }

        Attribute const* from_attr = from_node->find_attr(from.attr);
        if (from_attr == nullptr)
        {
            return Connect::UnknownPin;
        }
        Attribute const* to_attr = to_node->find_attr(to.attr);
        if (to_attr == nullptr)
        {
            return Connect::UnknownPin;
        }

        if (from.node == to.node)
        {
            return Connect::SameNode;
        }

        if (from_attr->role != AttributeSpec::Role::Output)
        {
            return Connect::KindMismatch;
        }
        if (to_attr->role != AttributeSpec::Role::Input)
        {
            return Connect::KindMismatch;
        }

        if (not tc.compatible(from_attr->data_type, to_attr->data_type))
        {
            return Connect::TypeMismatch;
        }

        for (auto const& l : g.links())
        {
            if (l.to == to)
            {
                return Connect::AlreadyConnected;
            }
        }

        return Connect::Allow;
    }
}
