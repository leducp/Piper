#ifndef PIPER_TESTS_FIXTURES_BUILD_MOTOR_DUAL_JACOBIAN_H
#define PIPER_TESTS_FIXTURES_BUILD_MOTOR_DUAL_JACOBIAN_H

#include "piper/graph.h"
#include "piper/registry.h"

namespace piper::fixtures
{
    // Builds the motor_control_dual_jacobian example: two cartesian
    // targets feed a 2x2 jacobian, whose two outputs drive two
    // motors. Each motor's `command` lives in stage "control" and
    // its `measured` output lives in stage "feedback" (per-pin
    // override -- the Bus pattern). Two probes consume the feedback.
    // Throws std::runtime_error if a required builtin is missing.
    Graph build_motor_dual_jacobian(NodeRegistry const& reg);
}

#endif
