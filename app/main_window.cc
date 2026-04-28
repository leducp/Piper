#include "piper/app/main_window.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include <imgui.h>

#include "piper/app/theme_loader.h"
#include "piper/builtin_nodes.h"
#include "piper/serialize_v2.h"

namespace piper::app
{
    MainWindow::MainWindow()
        : adapter_(graph_, registry_, theme_)
        , editor_(adapter_)
    {
        register_builtin_nodes(registry_);
        try_load_theme();
        apply_current_theme();
        adapter_.rebuild();
    }

    bool MainWindow::load_file(std::string const& path)
    {
        std::ifstream f(path);
        if (not f.is_open())
        {
            std::fprintf(stderr, "could not open %s\n", path.c_str());
            return false;
        }
        std::ostringstream buf;
        buf << f.rdbuf();
        try
        {
            auto result = v2::deserialize(buf.str(), registry_);
            graph_       = std::move(result.graph);
            diagnostics_ = std::move(result.diagnostics);
        }
        catch (std::exception const& e)
        {
            std::fprintf(stderr, "load failed: %s\n", e.what());
            return false;
        }
        loaded_path_ = path;
        adapter_.rebuild();

        for (auto const& d : diagnostics_)
        {
            std::fprintf(stderr, "diagnostic: %s\n", d.message.c_str());
        }
        return true;
    }

    void MainWindow::try_load_theme()
    {
        char const* candidates[] = {
            "data/theme.json",
            "../data/theme.json",
            "../../data/theme.json",
        };
        for (char const* p : candidates)
        {
            std::error_code ec;
            if (not std::filesystem::exists(p, ec))
            {
                continue;
            }
            try
            {
                auto result = piper::load_theme(p);
                theme_       = std::move(result.theme);
                theme_path_  = p;
                theme_mtime_ = std::filesystem::last_write_time(p, ec);
                for (auto const& d : result.diagnostics)
                {
                    std::fprintf(stderr, "theme: %s\n", d.message.c_str());
                }
                return;
            }
            catch (std::exception const& e)
            {
                std::fprintf(stderr, "theme load (%s) failed: %s\n", p, e.what());
            }
        }
        std::fprintf(stderr, "theme: no data/theme.json found, using defaults\n");
    }

    void MainWindow::apply_current_theme()
    {
        apply_theme(theme_, canvas_style_, ImGui::GetStyle());
        editor_.set_style(canvas_style_);
    }

    void MainWindow::poll_theme_reload()
    {
        if (theme_path_.empty())
        {
            return;
        }
        auto const now = std::chrono::steady_clock::now();
        if (now - theme_last_check_ < std::chrono::seconds(1))
        {
            return;
        }
        theme_last_check_ = now;

        std::error_code ec;
        auto const mt = std::filesystem::last_write_time(theme_path_, ec);
        if (ec or mt == theme_mtime_)
        {
            return;
        }
        theme_mtime_ = mt;
        try
        {
            auto result = piper::load_theme(theme_path_);
            theme_      = std::move(result.theme);
            apply_current_theme();
            adapter_.rebuild();
            for (auto const& d : result.diagnostics)
            {
                std::fprintf(stderr, "theme reload: %s\n", d.message.c_str());
            }
        }
        catch (std::exception const& e)
        {
            std::fprintf(stderr, "theme reload failed: %s\n", e.what());
        }
    }

    bool MainWindow::draw()
    {
        poll_theme_reload();

        ImGuiViewport const* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("##piper_root",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar     |
                     ImGuiWindowFlags_NoResize       |
                     ImGuiWindowFlags_NoMove         |
                     ImGuiWindowFlags_NoCollapse     |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_MenuBar);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Quit", "Ctrl+Q"))
                {
                    running_ = false;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help"))
            {
                ImGui::MenuItem("About Piper", nullptr, false, false);
                ImGui::EndMenu();
            }
            if (not loaded_path_.empty())
            {
                ImGui::Text("  %s", loaded_path_.c_str());
            }
            ImGui::EndMenuBar();
        }

        editor_.draw(ImGui::GetContentRegionAvail());
        (void)editor_.consume_events();

        ImGui::End();

        ImGuiIO const& io = ImGui::GetIO();
        if (io.KeyCtrl and ImGui::IsKeyPressed(ImGuiKey_Q, false))
        {
            running_ = false;
        }
        return running_;
    }
}
