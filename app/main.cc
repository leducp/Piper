#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <cstdio>

#include "piper/app/activity.h"
#include "piper/app/main_window.h"

void glfw_error_callback(int error, char const* description)
{
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
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

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    piper::app::MainWindow main_window;
    if (argc > 1)
    {
        main_window.load_file(argv[1]);
    }

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
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
