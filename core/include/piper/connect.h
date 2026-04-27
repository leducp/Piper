#ifndef PIPER_CONNECT_H
#define PIPER_CONNECT_H

#include "piper/graph.h"
#include "piper/link.h"
#include "piper/type_check.h"

namespace piper
{
    enum class Connect
    {
        Allow,
        UnknownPin,
        SameNode,
        KindMismatch,        // from is not Output, or to is not Input
        TypeMismatch,
        AlreadyConnected,    // input pin already has an incoming link
    };

    Connect validate_connection(Graph const& g,
                                PinRef const& from,
                                PinRef const& to,
                                TypeCheck const& tc);
}

#endif
