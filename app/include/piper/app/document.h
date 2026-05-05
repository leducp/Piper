#ifndef PIPER_APP_DOCUMENT_H
#define PIPER_APP_DOCUMENT_H

#include <chrono>
#include <string>
#include <vector>

#include <imgui.h>

#include "piper/annotation.h"
#include "piper/app/canvas_adapter.h"
#include "piper/canvas/editor.h"
#include "piper/command_stack.h"
#include "piper/diagnostic.h"
#include "piper/graph.h"
#include "piper/label.h"
#include "piper/node.h"
#include "piper/registry.h"
#include "piper/theme.h"

namespace piper::studio
{
    // Popup / drag state owned per-document. Switching tabs cannot
    // corrupt another document by writing through MainWindow-singleton
    // ids: each doc carries its own popup pointers, and MainWindow's
    // popup pass runs only against the active doc (with a separate
    // sweep that coalesces / clears any popup left open on a doc that
    // is no longer active).
    struct PopupState
    {
        AnnotationId             editing_annotation{invalid_annotation_id};
        AnnotationId             annotation_buf_id{invalid_annotation_id};
        std::string              annotation_text_buf;
        // True between popup open and close: every command pushed
        // while open is folded into one undo step (CompositeCommand)
        // via CommandStack open_group / close_group.
        bool                     annotation_group_open{false};
        // Captured at popup open. Live-editing writes the buffer
        // directly into Annotation::text so the canvas updates per
        // keystroke; on close we restore this and push one command
        // (original -> final) so undo collapses the session.
        std::string              anno_original_text;
        // Inline-rename popup state. Used for both nodes and labels;
        // exactly one is active at a time. The buf is seeded once per
        // open via the matching `_buf_id` sentinel.
        NodeId                   editing_node{invalid_node_id};
        NodeId                   node_name_buf_id{invalid_node_id};
        std::string              node_name_buf;
        LabelId                  editing_label{invalid_label_id};
        LabelId                  label_name_buf_id{invalid_label_id};
        std::string              label_name_buf;
        bool                     label_group_open{false};
        // Annotation drag (host-owned via editor extra-drag events).
        // Active between ExtraDragStarted and ExtraDragEnded; the
        // command is pushed only on Ended so a single drag = one undo.
        AnnotationId             dragging_annotation{invalid_annotation_id};
        Point                    annotation_drag_start_pos{0.0f, 0.0f};
        ImVec2                   annotation_drag_start_canvas{0.0f, 0.0f};
    };

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

        PopupState                  popup;
    };
}

#endif
