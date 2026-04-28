#ifndef PIPER_APP_MAIN_WINDOW_H
#define PIPER_APP_MAIN_WINDOW_H

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "piper/app/canvas_adapter.h"
#include "piper/canvas/editor.h"
#include "piper/canvas/style.h"
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
        // to quit (File → Quit, Ctrl+Q).
        bool draw();

    private:
        // Tries a few common locations and applies the theme on
        // success. Silent no-op on failure (defaults remain).
        void try_load_theme();
        void poll_theme_reload();
        void apply_current_theme();

        piper::Theme        theme_;
        piper::NodeRegistry registry_;
        piper::Graph        graph_;
        PiperCanvasGraph    adapter_;
        canvas::Editor      editor_;
        canvas::Style       canvas_style_;

        std::vector<Diagnostic>             diagnostics_;
        std::string                         loaded_path_;
        std::string                         theme_path_;
        std::filesystem::file_time_type     theme_mtime_{};
        std::chrono::steady_clock::time_point theme_last_check_{};

        bool                running_{true};
    };
}

#endif
