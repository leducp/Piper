#ifndef PIPER_APP_MAIN_WINDOW_H
#define PIPER_APP_MAIN_WINDOW_H

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "piper/app/canvas_adapter.h"
#include "piper/app/panels/inspector_panel.h"
#include "piper/app/panels/modes_panel.h"
#include "piper/app/panels/stages_panel.h"
#include "piper/canvas/editor.h"
#include "piper/canvas/style.h"
#include "piper/command_stack.h"
#include "piper/diagnostic.h"
#include "piper/graph.h"
#include "piper/registry.h"
#include "piper/theme.h"

namespace piper::app
{
    class MainWindow
    {
    public:
        MainWindow();

        // Returns false on a hard load error (missing file / malformed
        // JSON / unsupported version). Soft drift is reported through
        // diagnostics() and does not fail the load.
        bool load_file(std::string const& path);

        std::vector<Diagnostic> const& diagnostics() const { return diagnostics_; }

        // Renders the current frame. Call between ImGui::NewFrame
        // and ImGui::Render. Returns false when the user has asked
        // to quit (File -> Quit, Ctrl+Q).
        bool draw();

        // True when the render loop should keep polling rather than
        // blocking on glfwWaitEventsTimeout (e.g. stage auto-play
        // needs frames to advance the timer).
        bool wants_continuous_render() const { return stage_play_active_; }

    private:
        // Tries a few common locations and applies the theme on
        // success. Silent no-op on failure (defaults remain).
        void try_load_theme();
        void poll_theme_reload();
        void apply_current_theme();

        // Snapshot the current selection into clipboard_. Stores
        // node positions relative to the selection's bounding box
        // so paste keeps the cluster shape around the cursor.
        void copy_to_clipboard(std::span<canvas::NodeId const> ids);

        // Spawns nodes + internal links from clipboard_ around the
        // given canvas-space cursor. Returns true if anything was
        // pasted. Caller is responsible for the surrounding group.
        bool paste_from_clipboard(ImVec2 const& at_canvas);

        // Resets graph + selection + active stage/profile to a clean
        // empty document. Adapter rebuilds.
        void new_document();

        // Spawns a node of the given type at the canvas-space pos
        // through AddNodeCommand. Generates a unique name based on
        // the type so the user does not collide with existing nodes.
        void add_node_at(piper::NodeType const& type, ImVec2 const& canvas_pos);

        // Writes the current graph to `path` as V2 JSON. Updates
        // loaded_path_ on success. Returns false on I/O error.
        bool save_to(std::string const& path);

        // Recomputes lint_diagnostics_ from the current graph state.
        // Cheap O(N + L) sweep; called once per draw().
        void recompute_lints();

        // Stage cycling. goto_next/prev wrap around the stage list.
        // Toggle starts/stops auto-advance at ~0.5 Hz so the user
        // can visualize the engine order at a glance.
        void goto_next_stage();
        void goto_prev_stage();
        void toggle_stage_play();
        void tick_stage_play();

        piper::Theme        theme_;
        piper::NodeRegistry registry_;
        piper::Graph        graph_;
        piper::CommandStack command_stack_;
        PiperCanvasGraph    adapter_;
        canvas::Editor      editor_;
        canvas::Style       canvas_style_;
        InspectorPanel      inspector_;
        StagesPanel         stages_panel_;
        ModesPanel          modes_panel_;
        std::string         current_stage_;
        std::string         active_mode_profile_;

        std::vector<NodeId>                 selection_;
        // Diagnostics from the last load (drift, schema, orphan refs).
        // Persistent until the next load_file / new_document.
        std::vector<Diagnostic>             diagnostics_;
        // Live lints recomputed each frame from the current graph
        // state (disconnected nodes, missing stage, unconnected
        // inputs, mode-profile coverage).
        std::vector<Diagnostic>             lint_diagnostics_;

        // In-memory clipboard for cut/copy/paste. Stored as Node
        // values + their relative offset so paste places them
        // around the cursor.
        struct ClipboardEntry
        {
            Node  node;
            Point relative_pos;
        };
        struct Clipboard
        {
            std::vector<ClipboardEntry> nodes;
            std::vector<Link>           internal_links;
            Point                       origin;
        };
        Clipboard                           clipboard_;


        std::string                         loaded_path_;
        std::string                         theme_path_;
        std::filesystem::file_time_type     theme_mtime_{};
        std::chrono::steady_clock::time_point theme_last_check_{};

        float               inspector_width_{340.0f};
        // Min width is sized so all 4 tabs fit horizontally.
        float               inspector_min_width_{300.0f};
        bool                inspector_visible_{true};

        // Stage play state. ~0.5 Hz auto-advance through stages.
        bool                                  stage_play_active_{false};
        std::chrono::steady_clock::time_point stage_play_next_advance_{};

        bool                running_{true};
    };
}

#endif
