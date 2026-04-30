#ifndef PIPER_ENGINE_DIAGNOSTIC_H
#define PIPER_ENGINE_DIAGNOSTIC_H

#include <string>

#include "piper/link.h"
#include "piper/node.h"

namespace piper::engine
{
    struct BuildDiagnostic
    {
        enum class Event
        {
            UnknownStepFactory,    // graph references a type with no factory
            UnresolvedInput,       // link references an input or output that the step did not declare
            TypeMismatchAtLink,    // producer pin type != consumer pin type
            CycleDetected,         // per-stage subgraph has a cycle
            UnknownStageOnPin,     // pin lists a stage that the graph does not own
            NodeNeverScheduled,    // active_stages is empty
            StepDeclareIoFailed,   // declare_io() threw (e.g. malformed member value)
        };

        Event         event{Event::UnresolvedInput};
        std::string   message;
        piper::NodeId node_id{piper::invalid_node_id};
        std::string   attr_name;
        piper::LinkId link_id{piper::invalid_link_id};
    };
}

#endif
