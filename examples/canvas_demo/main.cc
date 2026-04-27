#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <cstdio>
#include <span>
#include <vector>

#include "piper/canvas/editor.h"
#include "piper/canvas/graph.h"

namespace
{
    using namespace piper::canvas;

    // Hardcoded 3-node demo graph. Pins are empty (PR 2.4 adds pins
    // and link rendering). Body colors deliberately differ so the
    // user can see each node distinctly while panning/zooming.
    class DemoGraph : public Graph
    {
    public:
        DemoGraph()
        {
            nodes_.push_back(Node{
                NodeId{1}, "Source", { 50.0f, 100.0f },
                IM_COL32(0x40, 0x80, 0xC0, 0xFF),
                IM_COL32(0x2A, 0x2A, 0x2A, 0xFF),
                1.0f, { 0.0f, 0.0f }, {}, {},
            });
            nodes_.push_back(Node{
                NodeId{2}, "Filter", { 280.0f, 100.0f },
                IM_COL32(0x40, 0xC0, 0x80, 0xFF),
                IM_COL32(0x2A, 0x2A, 0x2A, 0xFF),
                1.0f, { 0.0f, 0.0f }, {}, {},
            });
            nodes_.push_back(Node{
                NodeId{3}, "Sink", { 510.0f, 100.0f },
                IM_COL32(0xC0, 0x40, 0x80, 0xFF),
                IM_COL32(0x2A, 0x2A, 0x2A, 0xFF),
                1.0f, { 0.0f, 0.0f }, {}, {},
            });
        }

        std::span<Node const> nodes() const override { return nodes_; }
        std::span<Link const> links() const override { return {}; }

    private:
        std::vector<Node> nodes_;
    };

    void glfw_error_callback(int error, char const* description)
    {
        std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
    }
}

int main()
{
    glfwSetErrorCallback(glfw_error_callback);
    if (not glfwInit())
    {
        return 1;
    }

    char const* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(960, 600, "piper canvas demo", nullptr, nullptr);
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
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    DemoGraph graph;
    Editor    editor{graph};

    while (not glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);

        ImGui::Begin("piper canvas demo",
                     nullptr,
                     ImGuiWindowFlags_NoTitleBar |
                     ImGuiWindowFlags_NoResize   |
                     ImGuiWindowFlags_NoMove     |
                     ImGuiWindowFlags_NoCollapse);

        editor.draw(ImGui::GetContentRegionAvail());

        ImGui::End();

        ImGui::Render();
        int display_w = 0;
        int display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.10f, 0.10f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
