#ifndef PIPER_TYPES_H
#define PIPER_TYPES_H

namespace piper
{
    // Possible modes of a node during execution
    enum Mode
    {
        enable,  // The node is enable and run at its nominal behavior
        disable, // The node is disable and shall not interact with the pipeline (dead end)
        neutral  // The node shall interact with the pipeline in a neutral step (i.e. passthrough, add 0, multiply with 1, etc.)
    };
}

#endif
