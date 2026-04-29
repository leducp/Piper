#ifndef PIPER_APP_DOCUMENT_H
#define PIPER_APP_DOCUMENT_H

#include <string>
#include <vector>

#include "piper/app/canvas_adapter.h"
#include "piper/canvas/editor.h"
#include "piper/command_stack.h"
#include "piper/diagnostic.h"
#include "piper/graph.h"
#include "piper/registry.h"
#include "piper/theme.h"

namespace piper::app
{
    // One open pipeline. Held inside MainWindow via std::unique_ptr so
    // the address is stable -- the canvas::Editor and PiperCanvasGraph
    // bind references to this Document's `graph` and the shared theme
    // and registry, and any reallocation would invalidate them.
    struct Document
    {
        Document(piper::Theme const&        theme,
                 piper::NodeRegistry const& registry);

        Document(Document const&)            = delete;
        Document& operator=(Document const&) = delete;
        Document(Document&&)                 = delete;
        Document& operator=(Document&&)      = delete;

        piper::Graph                graph;
        piper::CommandStack         command_stack;
        PiperCanvasGraph            adapter;
        canvas::Editor              editor;

        // Empty when the document has never been saved (untitled).
        std::string                 loaded_path;
        // Name within the bundle file (the JSON `pipelines[i].name`).
        // When multiple tabs share a `loaded_path`, this disambiguates
        // them and is preserved on save.
        std::string                 pipeline_name;
        std::vector<NodeId>         selection;
        std::string                 current_stage;
        std::string                 active_mode_profile;

        // Persistent until the next load / new_document.
        std::vector<Diagnostic>     diagnostics;
        // Recomputed each frame from the current graph state.
        std::vector<Diagnostic>     lint_diagnostics;

        // Set whenever a command is pushed; cleared on save.
        bool                        dirty = false;
    };
}

#endif
