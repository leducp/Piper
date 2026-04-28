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
        std::vector<Diagnostic>             diagnostics_;

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

        float               inspector_width_{280.0f};
        bool                running_{true};
    };
}

#endif
