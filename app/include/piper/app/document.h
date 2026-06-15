#ifndef PIPER_APP_DOCUMENT_H
#define PIPER_APP_DOCUMENT_H

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <imgui.h>

#include "piper/annotation.h"
#include "piper/app/canvas_adapter.h"
#include "piper/canvas/editor.h"
#include "piper/command_stack.h"
#include "piper/diagnostic.h"
#include "piper/engine/engine.h"
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

        // Live-engine state. Constructed when the user clicks Run on
        // the toolbar; ticked every frame while engine_running. The
        // engine is auto-rebuilt when command_stack.revision()
        // disagrees with engine_built_revision (graph mutated since
        // build); state resets on rebuild, which is fine for "tweak,
        // see" interactivity.
        std::unique_ptr<piper::engine::Engine>     engine;
        bool                                       engine_running{false};
        std::size_t                                engine_built_revision{0};
        // Hot-rebuild debounce: last revision seen by tick_engine_live
        // and the ImGui time it changed. Rebuild fires only once the
        // revision has been stable for a beat, so merged-command drags
        // (which bump the revision every frame) don't wipe probe state.
        std::size_t                                live_seen_revision{0};
        double                                     live_revision_change_time{0.0};
        // Parsed min/max of each external_input's slider, keyed by node
        // id. Rebuilt lazily when command_stack.revision() changes so
        // the Live panel doesn't re-parse attr strings every frame.
        struct LiveRange
        {
            float lo{0.0f};
            float hi{0.0f};
        };
        std::unordered_map<piper::NodeId, LiveRange> live_range_cache;
        std::size_t                                live_range_cache_revision{std::size_t(-1)};
        // Per-tick capture of every external_output<float>'s latest
        // value, keyed by node id. Canvas body renderer reads it to
        // draw probe values inline.
        std::unordered_map<piper::NodeId, float>   probe_latest;
        // Scrolling history of each probe, used by the Live panel for
        // a tiny line plot. Keyed by node id; bounded length.
        std::unordered_map<piper::NodeId, std::vector<float>> probe_history;
        // Host-driven values for external_input nodes, keyed by the
        // node's "name" Member (the same key Engine::input<T> uses).
        // Persisted across run/stop so the user's slider positions
        // survive a rebuild. Pushed to the engine each tick before
        // play() while the engine is running.
        std::unordered_map<std::string, float>     live_input_float;
        std::unordered_map<std::string, int32_t>   live_input_int;
        // True: all probes overlaid on one plot (phase relationships
        // visible). False: one plot per probe (each gets its own
        // y-axis range, less crowding for probes at very different
        // scales). Toggled from the Live panel.
        bool                                       live_merge_probe_plots{true};
    };
}

#endif
