#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <cstdio>
#include <span>
#include <vector>

#include "piper/canvas/editor.h"
#include "piper/canvas/event.h"
#include "piper/canvas/graph.h"

using namespace piper::canvas;

constexpr ImU32    float_pin_color = IM_COL32(0x80, 0xC0, 0xFF, 0xFF);
constexpr ImU32    int_pin_color   = IM_COL32(0xFF, 0xC0, 0x80, 0xFF);
constexpr ImU32    link_color      = IM_COL32(0xC0, 0xC0, 0xC0, 0xFF);
constexpr ImU32    body_color      = IM_COL32(0x2A, 0x2A, 0x2A, 0xFF);
constexpr uint32_t float_tag       = 0x00666CD9u;
constexpr uint32_t int_tag         = 0x00465E12u;

class DemoGraph : public Graph
{
public:
    DemoGraph()
    {
        // Pre-size pin storage so vectors are not reallocated after
        // node spans are constructed.
        inputs_.resize(5);
        outputs_.resize(5);
        for (auto& v : inputs_)
        {
            v.reserve(4);
        }
        for (auto& v : outputs_)
        {
            v.reserve(4);
        }

        outputs_[0].push_back(Pin{ PinId{1}, PinKind::Output, "out_a", float_pin_color, float_tag });
        outputs_[0].push_back(Pin{ PinId{2}, PinKind::Output, "out_b", float_pin_color, float_tag });

        inputs_[1].push_back(Pin{ PinId{3}, PinKind::Input,  "in_a",  float_pin_color, float_tag });
        inputs_[1].push_back(Pin{ PinId{4}, PinKind::Input,  "in_b",  float_pin_color, float_tag });
        outputs_[1].push_back(Pin{ PinId{5}, PinKind::Output, "out",   float_pin_color, float_tag });

        inputs_[2].push_back(Pin{ PinId{6}, PinKind::Input,  "in_a",  float_pin_color, float_tag });
        inputs_[2].push_back(Pin{ PinId{7}, PinKind::Input,  "in_b",  float_pin_color, float_tag });

        inputs_[3].push_back(Pin{ PinId{8}, PinKind::Input,  "in",    float_pin_color, float_tag });

        outputs_[4].push_back(Pin{ PinId{9}, PinKind::Output, "count", int_pin_color,   int_tag });

        nodes_.push_back(Node{
            NodeId{1}, "Source", { 50.0f, 100.0f },
            IM_COL32(0x40, 0x80, 0xC0, 0xFF), body_color,
            1.0f, { 0.0f, 0.0f }, {}, outputs_[0],
        });
        nodes_.push_back(Node{
            NodeId{2}, "Filter", { 280.0f, 100.0f },
            IM_COL32(0x40, 0xC0, 0x80, 0xFF), body_color,
            1.0f, { 0.0f, 0.0f }, inputs_[1], outputs_[1],
        });
        nodes_.push_back(Node{
            NodeId{3}, "Sink", { 510.0f, 100.0f },
            IM_COL32(0xC0, 0x40, 0x80, 0xFF), body_color,
            1.0f, { 0.0f, 0.0f }, inputs_[2], {},
        });
        nodes_.push_back(Node{
            NodeId{4}, "Probe", { 510.0f, 260.0f },
            IM_COL32(0xC0, 0xC0, 0x40, 0xFF), body_color,
            1.0f, { 0.0f, 0.0f }, inputs_[3], {},
        });
        nodes_.push_back(Node{
            NodeId{5}, "Counter", { 50.0f, 260.0f },
            IM_COL32(0x80, 0x80, 0x80, 0xFF), body_color,
            1.0f, { 0.0f, 0.0f }, {}, outputs_[4],
        });

        links_.push_back(Link{ LinkId{1}, PinId{1}, PinId{3}, link_color });
        links_.push_back(Link{ LinkId{2}, PinId{5}, PinId{6}, link_color });
        next_link_id_ = 3;
    }

    std::span<Node const> nodes() const override { return nodes_; }
    std::span<Link const> links() const override { return links_; }

    Connect can_connect(Pin const& a, Pin const& b) const override
    {
        if (a.type_tag != b.type_tag)
        {
            return Connect::TypeMismatch;
        }
        Pin const* input = &a;
        if (b.kind == PinKind::Input)
        {
            input = &b;
        }
        for (auto const& l : links_)
        {
            if (l.to == input->id)
            {
                return Connect::AlreadyConnected;
            }
        }
        return Connect::Allow;
    }

    void apply(Event const& ev)
    {
        if (ev.kind == EventKind::NodeMoved)
        {
            for (auto& n : nodes_)
            {
                if (n.id == ev.node)
                {
                    n.pos = ev.pos;
                    break;
                }
            }
        }
        else if (ev.kind == EventKind::LinkCreated)
        {
            links_.push_back(Link{
                LinkId{next_link_id_++},
                ev.pin_from, ev.pin_to,
                link_color,
            });
        }
    }

private:
    std::vector<std::vector<Pin>> inputs_;
    std::vector<std::vector<Pin>> outputs_;
    std::vector<Node>             nodes_;
    std::vector<Link>             links_;
    uint64_t                      next_link_id_{1};
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
        for (auto const& ev : editor.consume_events())
        {
            graph.apply(ev);
            if (ev.kind == EventKind::SelectionChanged)
            {
                std::printf("selection: %zu node(s)\n", ev.selection.size());
            }
            else if (ev.kind == EventKind::LinkCreated)
            {
                std::printf("link: %llu -> %llu\n",
                            (unsigned long long)ev.pin_from.v,
                            (unsigned long long)ev.pin_to.v);
            }
        }

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
