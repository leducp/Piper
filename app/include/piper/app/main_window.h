#ifndef PIPER_APP_MAIN_WINDOW_H
#define PIPER_APP_MAIN_WINDOW_H

#include <string>
#include <vector>

#include "piper/app/canvas_adapter.h"
#include "piper/canvas/editor.h"
#include "piper/diagnostic.h"
#include "piper/graph.h"
#include "piper/registry.h"

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
        piper::NodeRegistry registry_;
        piper::Graph        graph_;
        PiperCanvasGraph    adapter_;
        canvas::Editor      editor_;
        std::vector<Diagnostic> diagnostics_;
        std::string         loaded_path_;
        bool                running_{true};
    };
}

#endif
