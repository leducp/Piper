#include <imgui.h>
#include <implot.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

#include "piper/app/activity.h"
#include "piper/app/bundled_fonts.h"
#include "piper/app/main_window.h"
#include "piper/app/settings.h"

void glfw_error_callback(int error, char const* description)
{
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Escape hatch for dense panels that report 1.0 content scale: an
// explicit PIPER_SCALE wins over the detected value when set positive.
float apply_scale_override(float scale)
{
    if (char const* env = std::getenv("PIPER_SCALE"))
    {
        float const s = float(std::atof(env));
        if (s > 0.0f)
        {
            return s;
        }
    }
    return scale;
}

constexpr std::string_view kBundledPrefix = "bundled:";
constexpr char const*       kDefaultBundled = "DejaVuSansMono";

ImFont* try_load_bundled(ImGuiIO& io, std::string_view name, float px)
{
    for (auto const& bf : piper::studio::bundled_fonts())
    {
        if (name == bf.name)
        {
            ImFontConfig cfg;
            cfg.FontDataOwnedByAtlas = false;
            return io.Fonts->AddFontFromMemoryTTF(
                const_cast<unsigned char*>(bf.data),
                int(bf.size), px, &cfg);
        }
    }
    return nullptr;
}

void load_font(ImGuiIO& io, std::string const& path, float size, float dpi_scale)
{
    io.Fonts->Clear();
    float const px = size * dpi_scale;
    ImFont* loaded = nullptr;

    if (path.starts_with(kBundledPrefix))
    {
        std::string_view const name{ path.data() + kBundledPrefix.size(),
                                     path.size() - kBundledPrefix.size() };
        loaded = try_load_bundled(io, name, px);
        if (loaded == nullptr)
        {
            std::fprintf(stderr, "font: bundled '%.*s' not found\n",
                         int(name.size()), name.data());
        }
    }
    else if (not path.empty())
    {
        loaded = io.Fonts->AddFontFromFileTTF(path.c_str(), px);
        if (loaded == nullptr)
        {
            std::fprintf(stderr, "font: cannot load '%s'\n", path.c_str());
        }
    }
    else
    {
        loaded = try_load_bundled(io, kDefaultBundled, px);
    }

    if (loaded == nullptr)
    {
        ImFontConfig cfg;
        cfg.SizePixels = px;
        io.Fonts->AddFontDefault(&cfg);
    }
}

int main(int argc, char** argv)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (not glfwInit())
    {
        return 1;
    }

    char const* glsl_version = "#version 130";
#if defined(__APPLE__)
    // macOS exposes only a 3.2+ core profile (or a legacy 2.1 context);
    // the 3.0 compatibility context requested below does not exist there,
    // and the GL3 backend needs a core profile with #version 150.
    glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    piper::studio::Settings const startup_settings = piper::studio::load_settings();
    // Query the primary monitor's content scale before the window exists
    // so a first-run default size grows with HiDPI -- a fixed 1280x800
    // clips the DPI-scaled UI on a 2x display. A saved size already
    // reflects the user's choice, so only the default is scaled.
    float init_scale = 1.0f;
    if (GLFWmonitor* monitor = glfwGetPrimaryMonitor())
    {
        float sx = 1.0f;
        float sy = 1.0f;
        glfwGetMonitorContentScale(monitor, &sx, &sy);
        if (sx > 0.0f)
        {
            init_scale = sx;
        }
    }
    init_scale = apply_scale_override(init_scale);
    int const init_w = startup_settings.window_w.value_or(int(1280 * init_scale));
    int const init_h = startup_settings.window_h.value_or(int(800 * init_scale));
    GLFWwindow* window = glfwCreateWindow(init_w, init_h, "Piper", nullptr, nullptr);
    if (window == nullptr)
    {
        glfwTerminate();
        return 1;
    }
    // Wayland refuses positioning entirely (the compositor decides);
    // calling glfwSetWindowPos there logs a 65548 / FEATURE_UNAVAILABLE
    // error. Skip the call on Wayland.
    if (startup_settings.window_x.has_value() and startup_settings.window_y.has_value()
        and glfwGetPlatform() != GLFW_PLATFORM_WAYLAND)
    {
        glfwSetWindowPos(window, *startup_settings.window_x, *startup_settings.window_y);
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // NavEnableKeyboard binds Alt to "focus the menu bar" and arrows
    // to traverse it, which steals our Alt+Arrow stage shortcut. We
    // do not need ImGui's keyboard nav (Tab/gamepad traversal), so
    // leave it off.

    float xscale = 1.0f;
    float yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    float dpi_scale = xscale;
    if (dpi_scale <= 0.0f)
    {
        dpi_scale = 1.0f;
    }
    dpi_scale = apply_scale_override(dpi_scale);

    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(dpi_scale);

    piper::studio::MainWindow main_window{dpi_scale};
    load_font(io, main_window.theme().font_path,
              main_window.theme().font_size, dpi_scale);
    for (int i = 1; i < argc; ++i)
    {
        main_window.load_file(argv[i]);
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    piper::studio::Activity activity;
    activity.boost();

    bool running = true;
    while (running)
    {
        if (activity.active() or main_window.wants_continuous_render())
        {
            glfwPollEvents();
        }
        else
        {
            glfwWaitEventsTimeout(0.5);
        }

        // Title-bar X / Alt-F4 reach us as glfwWindowShouldClose; fold
        // them into the in-app quit flow so we can prompt about unsaved
        // changes. Reset the flag so the loop keeps spinning while the
        // user is in the confirmation dialog.
        if (glfwWindowShouldClose(window))
        {
            glfwSetWindowShouldClose(window, GLFW_FALSE);
            main_window.request_quit();
        }

        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (io.MouseDelta.x != 0.0f or io.MouseDelta.y != 0.0f
            or io.MouseWheel != 0.0f
            or ImGui::IsAnyMouseDown()
            or ImGui::IsAnyItemActive())
        {
            activity.boost();
        }

        running = main_window.draw();

        ImGui::Render();
        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.10f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        activity.tick();

        std::string new_font_path;
        float       new_font_size = 16.0f;
        if (main_window.consume_font_reload(new_font_path, new_font_size))
        {
            load_font(io, new_font_path, new_font_size, dpi_scale);
        }
    }

    {
        int ww = 0;
        int wh = 0;
        glfwGetWindowSize(window, &ww, &wh);
        piper::studio::Settings shutdown;
        if (ww > 0 and wh > 0)
        {
            shutdown.window_w = ww;
            shutdown.window_h = wh;
        }
        // glfwGetWindowPos returns 0,0 on Wayland (compositor decides
        // positioning); skip persisting on that platform so we don't
        // poison the settings file.
        if (glfwGetPlatform() != GLFW_PLATFORM_WAYLAND)
        {
            int wx = 0;
            int wy = 0;
            glfwGetWindowPos(window, &wx, &wy);
            shutdown.window_x = wx;
            shutdown.window_y = wy;
        }
        piper::studio::save_settings(shutdown);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
