#ifndef PIPER_APP_MAIN_WINDOW_H
#define PIPER_APP_MAIN_WINDOW_H

#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "piper/app/document.h"
#include "piper/app/panels/inspector_panel.h"
#include "piper/app/panels/modes_panel.h"
#include "piper/app/panels/stages_panel.h"
#include "piper/canvas/cull.h"
#include "piper/canvas/style.h"
#include "piper/diagnostic.h"
#include "piper/registry.h"
#include "piper/theme.h"

namespace piper::app
{
    enum class ToastLevel { Info, Warn, Error };

    enum class AlignMode { Left, Right, Top, Bottom, CenterH, CenterV };

    class MainWindow
    {
    public:
        explicit MainWindow(float dpi_scale = 1.0f);

        // Queues a transient bottom-right notification. Free-threaded
        // not required: only the UI thread calls this.
        void push_toast(ToastLevel level, std::string message);

        // Returns false on a hard load error (missing file / malformed
        // JSON / unsupported version). Soft drift is reported through
        // the active document's diagnostics() and does not fail the
        // load. The loaded graph appears as a new tab.
        bool load_file(std::string const& path);

        std::vector<Diagnostic> const& diagnostics() const;

        // Renders the current frame. Call between ImGui::NewFrame
        // and ImGui::Render. Returns false when the user has asked
        // to quit (File -> Quit, Ctrl+Q).
        bool draw();

        // True when the render loop should keep polling rather than
        // blocking on glfwWaitEventsTimeout (e.g. stage auto-play
        // needs frames to advance the timer).
        bool wants_continuous_render() const { return stage_play_active_; }

        // Read-only theme access for the host (e.g. to load fonts at
        // startup before the first frame).
        piper::Theme const& theme() const { return theme_; }

        // Drain any pending font change. Returns true on the frame the
        // host should rebuild the ImGui font atlas; out-params hold the
        // requested path (empty = ImGui default) and pixel size.
        bool consume_font_reload(std::string& path, float& size);

    private:
        // ---- Document lifecycle ----
        Document*       active();
        Document const* active() const;

        // Adds a fresh untitled document and makes it active.
        Document& add_untitled_document();

        // Wires editor callbacks (context menu, body renderer, ...) for
        // a freshly-created document. The lambdas capture `this` and
        // `&doc` so they can reach both shared (registry, theme) and
        // per-doc (graph, command_stack, ...) state.
        void wire_document_callbacks(Document& doc);

        // Drains the active document's editor events and applies them
        // through the document's command stack.
        void process_editor_events(Document& doc);

        // Tab title shown in the tab bar: file basename or "untitled-N",
        // with a trailing "*" when dirty.
        std::string tab_title(Document const& doc, int idx) const;

        // Close attempt that may pop a "save changes?" prompt for
        // dirty docs. The actual erase happens in process_pending_close.
        void request_close(int idx);
        void process_pending_close();

        void try_load_theme();
        void poll_theme_reload();
        void apply_current_theme();

        void copy_to_clipboard(Document& doc, std::span<canvas::NodeId const> ids);
        bool paste_from_clipboard(Document& doc, ImVec2 const& at_canvas);

        // Spawns a node of the given type at the canvas-space pos in
        // the given document, through AddNodeCommand.
        void add_node_at(Document& doc,
                         piper::NodeType const& type,
                         ImVec2 const& canvas_pos);

        // Writes the active document to `path` as V2 JSON. Updates the
        // document's loaded_path on success. Returns false on I/O error.
        bool save_to(Document& doc, std::string const& path);

        // Recomputes lint_diagnostics on the active document. Cheap
        // O(N + L) sweep; called once per draw().
        void recompute_lints(Document& doc);

        void align_selection(Document& doc, AlignMode mode);
        void distribute_selection(Document& doc, bool horizontal);

        void poll_autosave();
        void autosave_doc(Document& doc);
        void clear_autosave(Document& doc);

        // Renders a graph overview in the canvas pane's bottom-right.
        // Click/drag pans the editor; no-op when the graph is empty.
        void draw_minimap(Document& doc);

        // Stage cycling on the active document.
        void goto_next_stage(Document& doc);
        void goto_prev_stage(Document& doc);
        void toggle_stage_play();
        void tick_stage_play(Document& doc);

        piper::Theme          theme_;
        piper::NodeRegistry   registry_;
        canvas::Style         canvas_style_;
        canvas::LayoutMetrics canvas_layout_;
        float                 dpi_scale_{1.0f};
        bool                     wants_font_reload_{false};
        bool                     font_picker_open_{false};
        bool                     about_open_{false};
        int                      about_selected_{0};
        bool                     shortcuts_open_{false};
        bool                     find_open_{false};
        bool                     find_focus_{false};
        std::array<char, 128>    find_buf_{};
        int                      find_selected_{0};
        std::vector<std::string> system_fonts_;
        bool                     system_fonts_scanned_{false};
        std::array<char, 64>     font_filter_buf_{};
        std::array<char, 512>    font_path_buf_{};
        float                    font_pending_size_{16.0f};
        InspectorPanel      inspector_;
        StagesPanel         stages_panel_;
        ModesPanel          modes_panel_;

        std::vector<std::unique_ptr<Document>> documents_;
        int                                    next_session_id_{1};
        std::chrono::steady_clock::time_point  autosave_last_check_{};
        // Index into documents_; -1 when the vector is empty (transient).
        int                                    active_doc_idx_{-1};
        // Set by request_close; consumed at end of frame.
        int                                    pending_close_idx_{-1};
        // While confirming, the doc index awaiting the user's choice.
        int                                    confirm_close_idx_{-1};
        // Counter for untitled-N naming.
        int                                    next_untitled_id_{1};
        // Set by Save dialog when triggered for a dirty doc that the
        // user wants to save before closing.
        int                                    save_then_close_idx_{-1};

        // Cross-document cut/copy clipboard. Pasting in a different tab
        // duplicates structure (ids regenerated through paste flow).
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

        std::string                         theme_path_;
        std::filesystem::file_time_type     theme_mtime_{
            std::filesystem::file_time_type::min()};
        std::chrono::steady_clock::time_point theme_last_check_{};

        float               inspector_width_{340.0f};
        float               inspector_min_width_{300.0f};
        bool                inspector_visible_{true};
        bool                minimap_visible_{true};

        bool                                  stage_play_active_{false};
        std::chrono::steady_clock::time_point stage_play_next_advance_{};

        bool                running_{true};

        struct Toast
        {
            std::string                           message;
            ToastLevel                            level{ToastLevel::Info};
            std::chrono::steady_clock::time_point spawned{};
        };
        std::vector<Toast>  toasts_;
    };
}

#endif
