#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <array>
#include <cstdio>
#include <span>
#include <vector>

#include "piper/canvas/editor.h"
#include "piper/canvas/graph.h"

using namespace piper::canvas;

constexpr ImU32 pin_color  = IM_COL32(0x80, 0xC0, 0xFF, 0xFF);
constexpr ImU32 link_color = IM_COL32(0xC0, 0xC0, 0xC0, 0xFF);
constexpr uint32_t float_tag = 0x00666CD9u;

class DemoGraph : public Graph
{
public:
    DemoGraph()
    {
        // Pin storage MUST be populated before nodes_ to keep
        // span addresses stable (std::array never reallocates).
        outputs_[0].push_back(Pin{ PinId{1}, PinKind::Output, "out", pin_color, float_tag });
        inputs_ [1].push_back(Pin{ PinId{2}, PinKind::Input,  "in",  pin_color, float_tag });
        outputs_[1].push_back(Pin{ PinId{3}, PinKind::Output, "out", pin_color, float_tag });
        inputs_ [2].push_back(Pin{ PinId{4}, PinKind::Input,  "in",  pin_color, float_tag });

        nodes_.push_back(Node{
            NodeId{1}, "Source", { 50.0f, 100.0f },
            IM_COL32(0x40, 0x80, 0xC0, 0xFF),
            IM_COL32(0x2A, 0x2A, 0x2A, 0xFF),
            1.0f, { 0.0f, 0.0f }, {}, outputs_[0],
        });
        nodes_.push_back(Node{
            NodeId{2}, "Filter", { 280.0f, 100.0f },
            IM_COL32(0x40, 0xC0, 0x80, 0xFF),
            IM_COL32(0x2A, 0x2A, 0x2A, 0xFF),
            1.0f, { 0.0f, 0.0f }, inputs_[1], outputs_[1],
        });
        nodes_.push_back(Node{
            NodeId{3}, "Sink", { 510.0f, 100.0f },
            IM_COL32(0xC0, 0x40, 0x80, 0xFF),
            IM_COL32(0x2A, 0x2A, 0x2A, 0xFF),
            1.0f, { 0.0f, 0.0f }, inputs_[2], {},
        });

        links_.push_back(Link{ LinkId{1}, PinId{1}, PinId{2}, link_color });
        links_.push_back(Link{ LinkId{2}, PinId{3}, PinId{4}, link_color });
    }

    std::span<Node const> nodes() const override { return nodes_; }
    std::span<Link const> links() const override { return links_; }

private:
    std::array<std::vector<Pin>, 3> inputs_;
    std::array<std::vector<Pin>, 3> outputs_;
    std::vector<Node>               nodes_;
    std::vector<Link>               links_;
};

void glfw_error_callback(int error, char const* description)
{
    std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
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
