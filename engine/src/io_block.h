#ifndef PIPER_ENGINE_SRC_IO_BLOCK_H
#define PIPER_ENGINE_SRC_IO_BLOCK_H

#include <any>
#include <cstddef>
#include <memory>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "piper/node.h"

#include "piper/engine/step.h"

namespace piper::engine
{
    struct OutputSlot
    {
        void*                 data{nullptr};
        std::type_info const* type{nullptr};
        std::any              ref_any;     // reference_wrapper<T const>(*data)
    };

    struct InputDecl
    {
        std::type_info const* type{nullptr};
    };

    // Per-step runtime block. Pointer to this is held by Step::io_ for
    // the templated input/output/member accessors. Engine references
    // and pointers into the maps remain valid across rehash because
    // unordered_map is node-based.
    struct Step::IoBlock
    {
        piper::NodeId                                 node_id{piper::invalid_node_id};
        std::shared_ptr<Step>                         step;
        std::unordered_map<std::string, OutputSlot>   outputs;
        std::unordered_map<std::string, InputDecl>    input_decls;
        std::unordered_map<std::string, std::any>     inputs;     // any: reference_wrapper<T const>
        std::unordered_map<std::string, std::string>  members;
        std::vector<std::size_t>                      active_stage_indices;
    };
}

#endif
