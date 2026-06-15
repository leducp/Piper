#ifndef PIPER_ENGINE_DIAGNOSTIC_H
#define PIPER_ENGINE_DIAGNOSTIC_H

#include <string>

#include "piper/link.h"
#include "piper/node.h"

namespace piper::engine
{
    struct BuildDiagnostic
    {
        enum class Kind
        {
            UnknownStepFactory,    // graph references a type with no factory
            UnresolvedInput,       // link references an input or output that the step did not declare
            TypeMismatchAtLink,    // producer pin type != consumer pin type
            CycleDetected,         // per-stage subgraph has a cycle
            UnknownStageOnPin,     // pin lists a stage that the graph does not own
            NodeNeverScheduled,    // active_stages is empty
            StepDeclareIoFailed,   // declare_io() threw (e.g. malformed member value)
            MissingInput,          // Required input pin has no wired producer
            DuplicateInputWiring,  // two links target the same input pin
            StepConstructionFailed,// factory threw while constructing the step
            FactoryTypeMismatch,   // external IO node's step is not the engine's Input/Output type
        };

        Kind          kind{Kind::UnresolvedInput};
        std::string   message;
        piper::NodeId node_id{piper::invalid_node_id};
        std::string   attr_name;
        piper::LinkId link_id{piper::invalid_link_id};
    };
}

#endif
