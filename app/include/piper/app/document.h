#ifndef PIPER_APP_DOCUMENT_H
#define PIPER_APP_DOCUMENT_H

#include <chrono>
#include <string>
#include <vector>

#include "piper/app/canvas_adapter.h"
#include "piper/canvas/editor.h"
#include "piper/command_stack.h"
#include "piper/diagnostic.h"
#include "piper/graph.h"
#include "piper/registry.h"
#include "piper/theme.h"

namespace piper::studio
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
        // Set when something invalidates `lint_diagnostics`. Recomputed
        // lazily by the host once per dirty cycle, not every frame.
        bool                        lint_dirty = true;

        // Per-session id used as the autosave filename stem. Stable for
        // the document's lifetime; assigned by MainWindow at construction.
        int                         session_id{0};
        // Path of this doc's autosave file, or empty if it hasn't been
        // autosaved yet. Cleared when a clean save deletes the file.
        std::string                 autosave_path;
        std::chrono::steady_clock::time_point last_autosave_at{};
    };
}

#endif
