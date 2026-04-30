#ifndef PIPER_TESTS_FIXTURES_TEST_HELPERS_H
#define PIPER_TESTS_FIXTURES_TEST_HELPERS_H

#include <vector>

#include "piper/diagnostic.h"
#include "piper/node_type.h"

namespace piper::fixtures
{
    bool any_of_event(std::vector<Diagnostic> const& diags, Diagnostic::Event e);

    NodeType make_adder();
    NodeType make_simple_type();
}

#endif
