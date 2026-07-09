#ifndef PIPER_APP_MAIN_WINDOW_H
#define PIPER_APP_MAIN_WINDOW_H

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

#include "piper/engine/builtin_steps.h"
#include "piper/engine/registry.h"
#include "piper/theme.h"

namespace piper::studio
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
        // to quit (File -> Quit, Ctrl+Q) AND any unsaved-changes
        // confirmation has been resolved.
        bool draw();

        // Begin a quit attempt from anywhere safe to call (including
        // outside an ImGui frame -- e.g. from main.cc when the GLFW
        // window-close button fires). If any document is dirty the
        // next draw() opens a confirmation popup; otherwise the next
        // draw() returns false.
        void request_quit();

        // True when the render loop should keep polling rather than
        // blocking on glfwWaitEventsTimeout (e.g. stage auto-play
        // needs frames to advance the timer).
        bool wants_continuous_render() const
        {
            if (stage_play_active_) { return true; }
            for (auto const& d : documents_)
            {
                if (d->engine_running) { return true; }
            }
            return false;
        }

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

        // Walk every open doc and clear UnknownNodeType diagnostics
        // whose type now resolves in the current registry, and mark
        // their adapter for rebuild + lint recompute. Called after a
        // node-pack load so previously-broken docs become usable
        // without a reload.
        void refresh_after_pack_load();

        // MRU list maintenance: prepend `path`, dedupe, truncate to 10,
        // persist to settings.
        void touch_recent_file(std::string const& path);


        // Stage cycling on the active document.
        void goto_next_stage(Document& doc);
        void goto_prev_stage(Document& doc);
        void toggle_stage_play();
        void tick_stage_play(Document& doc);

        // Live engine control on the active document. Toggle starts
        // and stops; tick rebuilds the engine if the graph has been
        // mutated since the last build, then ticks once.
        void toggle_engine_run(Document& doc);
        void tick_engine_live(Document& doc);
        void draw_live_panel(Document& doc);

        piper::Theme              theme_;
        piper::NodeRegistry       registry_;
        piper::engine::StepRegistry step_registry_;
        canvas::Style         canvas_style_;
        canvas::LayoutMetrics canvas_layout_;
        float                 dpi_scale_{1.0f};
        bool                     wants_font_reload_{false};
        bool                     font_picker_open_{false};
        bool                     about_open_{false};
        int                      about_selected_{0};
        bool                     autosave_recovery_open_{false};
        std::vector<std::string> autosave_pending_;
        bool                     shortcuts_open_{false};
        bool                     find_open_{false};
        bool                     find_focus_{false};
        std::string              find_buf_;
        int                      find_selected_{0};
        std::vector<std::string> system_fonts_;
        bool                     system_fonts_scanned_{false};
        std::string              font_filter_buf_;
        std::string              font_path_buf_;
        float                    font_pending_size_{16.0f};
        InspectorPanel      inspector_;
        StagesPanel         stages_panel_;
        ModesPanel          modes_panel_;

        std::vector<std::unique_ptr<Document>> documents_;
        int                                    next_session_id_{1};
        std::chrono::steady_clock::time_point  autosave_last_check_{};
        std::vector<std::string>               recent_files_;
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
        bool                color_pins_by_stage_{true};

        bool                                  stage_play_active_{false};
        std::chrono::steady_clock::time_point stage_play_next_advance_{};

        bool                running_{true};
        // request_quit() sets quit_requested_ from anywhere; the next
        // draw() either opens ##confirm_quit (if any doc is dirty) or
        // sets running_ = false straight away.
        bool                quit_requested_{false};
        bool                quit_confirming_{false};
        // One-shot: the next draw() should force the right-sidebar
        // tab bar to focus the "Live" tab. Set when toggle_engine_run
        // starts a run; consumed during tab construction.
        bool                focus_live_tab_pending_{false};

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
