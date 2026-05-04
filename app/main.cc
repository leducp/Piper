#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <cstdio>
#include <string>
#include <string_view>

#include "piper/app/activity.h"
#include "piper/app/bundled_fonts.h"
#include "piper/app/main_window.h"

void glfw_error_callback(int error, char const* description)
{
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

constexpr std::string_view kBundledPrefix = "bundled:";
constexpr char const*       kDefaultBundled = "DejaVuSansMono";

ImFont* try_load_bundled(ImGuiIO& io, std::string_view name, float px)
{
    for (auto const& bf : piper::app::bundled_fonts())
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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(1280, 800, "Piper", nullptr, nullptr);
    if (window == nullptr)
    {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
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

    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(dpi_scale);

    piper::app::MainWindow main_window{dpi_scale};
    load_font(io, main_window.theme().font_path,
              main_window.theme().font_size, dpi_scale);
    for (int i = 1; i < argc; ++i)
    {
        main_window.load_file(argv[i]);
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    piper::app::Activity activity;
    activity.boost();

    bool running = true;
    while (running and not glfwWindowShouldClose(window))
    {
        if (activity.active() or main_window.wants_continuous_render())
        {
            glfwPollEvents();
        }
        else
        {
            glfwWaitEventsTimeout(0.5);
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

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
