#ifndef PIPER_TESTS_FIXTURES_BUILD_MOTOR_SIMPLE_H
#define PIPER_TESTS_FIXTURES_BUILD_MOTOR_SIMPLE_H

#include "piper/graph.h"
#include "piper/registry.h"

namespace piper::fixtures
{
    // Builds the canonical motor_control_simple example: sin_wave ->
    // low_pass -> probe<float> across two stages, with a default mode
    // profile. Throws std::runtime_error if the registry is missing
    // any required builtin type.
    Graph build_motor_simple(NodeRegistry const& reg);
}

#endif
