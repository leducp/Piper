#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#define GL_SILENCE_DEPRECATION
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "piper/canvas/editor.h"
#include "piper/canvas/event.h"
#include "piper/canvas/graph.h"

using namespace piper::canvas;

constexpr ImU32    link_color = IM_COL32(0xC0, 0xC0, 0xC0, 0xFF);
constexpr ImU32    body_color = IM_COL32(0x2A, 0x2A, 0x2A, 0xFF);
constexpr uint32_t float_tag  = 0x00666CD9u;
constexpr uint32_t int_tag    = 0x00465E12u;

// Pastel color from a stable integer index using golden-ratio hue
// cycling (RTM convention). Storing the index -- not the color --
// lets reload reproduce colors deterministically. Real Piper would
// keep this in core/ alongside the type registry.
ImU32 pastel_from_index(int idx, float saturation = 0.45f, float value = 0.92f)
{
    constexpr float golden = 0.61803398875f;
    float hue = std::fmod(float(idx) * golden, 1.0f);
    if (hue < 0.0f)
    {
        hue += 1.0f;
    }
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    ImGui::ColorConvertHSVtoRGB(hue, saturation, value, r, g, b);
    return IM_COL32(int(r * 255.0f), int(g * 255.0f), int(b * 255.0f), 0xFF);
}

// Per-type hue index. Assigned at registration time; persists with
// the registry. New types just take next_hue_index_++.
constexpr int float_hue_idx = 0;
constexpr int int_hue_idx   = 1;
ImU32 const float_pin_color = pastel_from_index(float_hue_idx);
ImU32 const int_pin_color   = pastel_from_index(int_hue_idx);

class DemoGraph : public Graph
{
public:
    DemoGraph()
    {
        // Pre-size pin storage so vectors are not reallocated after
        // node spans are constructed.
        inputs_.resize(6);
        outputs_.resize(6);
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

        inputs_[3].push_back(Pin{ PinId{8},  PinKind::Input,  "value", float_pin_color, float_tag });
        inputs_[3].push_back(Pin{ PinId{10}, PinKind::Input,  "count", int_pin_color,   int_tag });

        outputs_[4].push_back(Pin{ PinId{9}, PinKind::Output, "count", int_pin_color,   int_tag });

        outputs_[5].push_back(Pin{ PinId{11}, PinKind::Output, "out", float_pin_color, float_tag });

        // body_min_size.y here is the extra content height *below
        // the pin rows*, sized so N field widgets fit at zoom 1.
        // Per field at zoom 1: ~13px label + 2px gap + 22px widget +
        // 4px inter-field gap = 41px; +6px outer padding.
        constexpr float field_h     = 41.0f;
        constexpr float field_pad_y = 6.0f;
        auto const extra = [&](int n_fields)
        {
            return ImVec2{ 220.0f, float(n_fields) * field_h + field_pad_y };
        };

        nodes_.push_back(Node{
            NodeId{1}, "Source", { 50.0f, 80.0f },
            IM_COL32(0x40, 0x80, 0xC0, 0xFF), {},
            body_color, 1.0f, extra(2), {}, outputs_[0],
        });
        nodes_.push_back(Node{
            NodeId{2}, "Filter", { 320.0f, 80.0f },
            IM_COL32(0x40, 0xC0, 0x80, 0xFF), {},
            body_color, 1.0f, extra(3), inputs_[1], outputs_[1],
        });
        nodes_.push_back(Node{
            NodeId{3}, "Sink", { 590.0f, 80.0f },
            IM_COL32(0xC0, 0x40, 0x80, 0xFF), {},
            body_color, 1.0f, extra(2), inputs_[2], {},
        });
        nodes_.push_back(Node{
            NodeId{4}, "Probe", { 590.0f, 320.0f },
            IM_COL32(0xC0, 0xC0, 0x40, 0xFF), {},
            body_color, 1.0f, extra(2), inputs_[3], {},
        });
        nodes_.push_back(Node{
            NodeId{5}, "Counter", { 50.0f, 320.0f },
            IM_COL32(0x80, 0x80, 0x80, 0xFF), {},
            body_color, 1.0f, extra(3), {}, outputs_[4],
        });
        // Field-less node: no extra content, no body_renderer entry.
        // Body sizes to a single pin row via min_body_height.
        nodes_.push_back(Node{
            NodeId{6}, "Const", { 320.0f, 320.0f },
            IM_COL32(0xA0, 0x60, 0x40, 0xFF), {},
            body_color, 1.0f, { 100.0f, 0.0f }, {}, outputs_[5],
        });

        links_.push_back(Link{ LinkId{1}, PinId{1}, PinId{3}, link_color });
        links_.push_back(Link{ LinkId{2}, PinId{5}, PinId{6}, link_color });
        next_link_id_ = 3;
        // Counter.count (int) -> Probe.count succeeds; -> Probe.value /
        // any float input is TypeMismatch.
    }

    std::span<Node const> nodes() const override { return nodes_; }
    std::span<Link const> links() const override { return links_; }

    Connect can_connect(Pin const& a, Pin const& b) const override
    {
        // Type mismatch is a structural reject -- engines need
        // compatible types to evaluate a link. Anything else (number
        // of links into the same input, fanout from one output) is
        // intentionally allowed: stage/mode/engine resolve which
        // source is live at runtime.
        if (a.type_tag != b.type_tag)
        {
            return Connect::TypeMismatch;
        }
        return Connect::Allow;
    }

    static bool mutates(EventKind k)
    {
        return k == EventKind::NodeMoved
            or k == EventKind::LinkCreated
            or k == EventKind::NodeDeleted
            or k == EventKind::PasteRequested;
    }

    void apply(Event const& ev)
    {
        switch (ev.kind)
        {
            case EventKind::NodeMoved:
            {
                for (auto& n : nodes_)
                {
                    if (n.id == ev.node)
                    {
                        n.pos = ev.pos;
                        break;
                    }
                }
                break;
            }
            case EventKind::LinkCreated:
            {
                links_.push_back(Link{
                    LinkId{next_link_id_++},
                    ev.pin_from, ev.pin_to,
                    link_color,
                });
                break;
            }
            case EventKind::NodeDeleted:
            {
                delete_node(ev.node);
                break;
            }
            case EventKind::CopyRequested:
            {
                copy(ev.selection);
                break;
            }
            case EventKind::PasteRequested:
            {
                paste(ev.pos);
                break;
            }
            default:
            {
                break;
            }
        }
    }

    // Snapshot the current graph state for undo. Caller pushes once
    // per atomic edit (e.g. one frame of consumed mutating events).
    void take_snapshot()
    {
        undo_stack_.push_back(snapshot());
        redo_stack_.clear();
    }

    bool undo()
    {
        if (undo_stack_.empty())
        {
            return false;
        }
        redo_stack_.push_back(snapshot());
        Snapshot s = std::move(undo_stack_.back());
        undo_stack_.pop_back();
        restore(s);
        return true;
    }

    bool redo()
    {
        if (redo_stack_.empty())
        {
            return false;
        }
        undo_stack_.push_back(snapshot());
        Snapshot s = std::move(redo_stack_.back());
        redo_stack_.pop_back();
        restore(s);
        return true;
    }

private:
    struct NodeRecord
    {
        NodeId           id;
        std::string_view title;
        ImVec2           pos;
        ImU32            header_color;
        ImU32            body_color;
        float            body_alpha;
        ImVec2           body_min_size;
        std::vector<Pin> inputs;
        std::vector<Pin> outputs;
    };

    struct Snapshot
    {
        std::vector<NodeRecord> nodes;
        std::vector<Link>       links;
        uint64_t                next_link_id;
        uint64_t                next_node_id;
        uint64_t                next_pin_id;
    };

    Snapshot snapshot() const
    {
        Snapshot s;
        s.nodes.reserve(nodes_.size());
        for (auto const& n : nodes_)
        {
            NodeRecord r{};
            r.id            = n.id;
            r.title         = n.title;
            r.pos           = n.pos;
            r.header_color  = n.header_color;
            r.body_color    = n.body_color;
            r.body_alpha    = n.body_alpha;
            r.body_min_size = n.body_min_size;
            r.inputs.assign (n.inputs.begin(),  n.inputs.end());
            r.outputs.assign(n.outputs.begin(), n.outputs.end());
            s.nodes.push_back(std::move(r));
        }
        s.links        = links_;
        s.next_link_id = next_link_id_;
        s.next_node_id = next_node_id_;
        s.next_pin_id  = next_pin_id_;
        return s;
    }

    void restore(Snapshot const& s)
    {
        std::size_t const n_count = s.nodes.size();
        inputs_.clear();
        outputs_.clear();
        nodes_.clear();
        inputs_.reserve(n_count);
        outputs_.reserve(n_count);
        nodes_.reserve(n_count);
        for (auto const& r : s.nodes)
        {
            inputs_.push_back(r.inputs);
            outputs_.push_back(r.outputs);
        }
        for (std::size_t i = 0; i < n_count; ++i)
        {
            auto const& r = s.nodes[i];
            nodes_.push_back(Node{
                r.id, r.title, r.pos,
                r.header_color, {},
                r.body_color, r.body_alpha, r.body_min_size,
                inputs_[i], outputs_[i],
            });
        }
        links_        = s.links;
        next_link_id_ = s.next_link_id;
        next_node_id_ = s.next_node_id;
        next_pin_id_  = s.next_pin_id;
    }

    struct ClipboardEntry
    {
        std::string_view title;
        ImVec2           pos;
        ImU32            header_color;
        ImU32            body_color;
        ImVec2           body_min_size;
        std::vector<Pin> inputs;
        std::vector<Pin> outputs;
    };

    void delete_node(NodeId id)
    {
        auto it = std::find_if(nodes_.begin(), nodes_.end(),
                               [&](Node const& n) { return n.id == id; });
        if (it == nodes_.end())
        {
            return;
        }
        std::vector<PinId> pin_ids;
        for (auto const& p : it->inputs)
        {
            pin_ids.push_back(p.id);
        }
        for (auto const& p : it->outputs)
        {
            pin_ids.push_back(p.id);
        }
        links_.erase(std::remove_if(links_.begin(), links_.end(),
                                    [&](Link const& l)
                                    {
                                        for (auto const pid : pin_ids)
                                        {
                                            if (l.from == pid or l.to == pid)
                                            {
                                                return true;
                                            }
                                        }
                                        return false;
                                    }),
                     links_.end());
        nodes_.erase(it);
    }

    void copy(std::span<NodeId const> ids)
    {
        clipboard_.clear();
        for (auto const id : ids)
        {
            auto it = std::find_if(nodes_.begin(), nodes_.end(),
                                   [&](Node const& n) { return n.id == id; });
            if (it == nodes_.end())
            {
                continue;
            }
            ClipboardEntry e{};
            e.title         = it->title;
            e.pos           = it->pos;
            e.header_color  = it->header_color;
            e.body_color    = it->body_color;
            e.body_min_size = it->body_min_size;
            e.inputs.assign (it->inputs.begin(),  it->inputs.end());
            e.outputs.assign(it->outputs.begin(), it->outputs.end());
            clipboard_.push_back(std::move(e));
        }
    }

    void paste(ImVec2 const& at_canvas)
    {
        if (clipboard_.empty())
        {
            return;
        }
        ImVec2 origin = clipboard_.front().pos;
        for (auto const& e : clipboard_)
        {
            if (e.pos.x < origin.x)
            {
                origin.x = e.pos.x;
            }
            if (e.pos.y < origin.y)
            {
                origin.y = e.pos.y;
            }
        }
        for (auto const& e : clipboard_)
        {
            std::size_t const slot = inputs_.size();
            inputs_.emplace_back();
            outputs_.emplace_back();
            inputs_.back().reserve(e.inputs.size());
            outputs_.back().reserve(e.outputs.size());
            for (auto const& p : e.inputs)
            {
                Pin np = p;
                np.id  = PinId{ next_pin_id_++ };
                inputs_[slot].push_back(np);
            }
            for (auto const& p : e.outputs)
            {
                Pin np = p;
                np.id  = PinId{ next_pin_id_++ };
                outputs_[slot].push_back(np);
            }
            ImVec2 const new_pos{
                at_canvas.x + (e.pos.x - origin.x),
                at_canvas.y + (e.pos.y - origin.y),
            };
            nodes_.push_back(Node{
                NodeId{ next_node_id_++ },
                e.title, new_pos,
                e.header_color, {},
                e.body_color, 1.0f, e.body_min_size,
                inputs_[slot], outputs_[slot],
            });
        }
    }

    std::vector<std::vector<Pin>> inputs_;
    std::vector<std::vector<Pin>> outputs_;
    std::vector<Node>             nodes_;
    std::vector<Link>             links_;
    std::vector<ClipboardEntry>   clipboard_;
    std::vector<Snapshot>         undo_stack_;
    std::vector<Snapshot>         redo_stack_;
    uint64_t                      next_link_id_{1};
    uint64_t                      next_node_id_{7};
    uint64_t                      next_pin_id_{12};
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

    // Per-node ephemeral widget state. Lives outside the graph so it
    // is not part of structural undo (matches typical engine behavior
    // where attribute edits use a separate command stream).
    enum class WidgetKind { Float, Int, Bool, Text };
    struct Field
    {
        WidgetKind  kind{WidgetKind::Float};
        std::string label;
        float       f{0.0f};
        int         i{0};
        bool        b{false};
        std::string txt{};
    };
    std::unordered_map<uint64_t, std::vector<Field>> widgets;
    widgets[1] = {
        Field{ WidgetKind::Float, "gain",       1.0f, 0, false, "" },
        Field{ WidgetKind::Float, "phase",      0.0f, 0, false, "" },
    };
    widgets[2] = {
        Field{ WidgetKind::Float, "cutoff",     0.25f, 0, false, "" },
        Field{ WidgetKind::Float, "resonance",  0.7f,  0, false, "" },
        Field{ WidgetKind::Bool,  "enabled",    0.0f,  0, true,  "" },
    };
    widgets[3] = {
        Field{ WidgetKind::Text,  "label",      0.0f, 0, false, "main" },
        Field{ WidgetKind::Float, "gain",       1.0f, 0, false, "" },
    };
    widgets[4] = {
        Field{ WidgetKind::Bool,  "active",     0.0f, 0, true,  "" },
        Field{ WidgetKind::Float, "threshold",  0.5f, 0, false, "" },
    };
    widgets[5] = {
        Field{ WidgetKind::Int,   "start",      0.0f, 0, false, "" },
        Field{ WidgetKind::Int,   "step",       0.0f, 1, false, "" },
        Field{ WidgetKind::Bool,  "wrap",       0.0f, 0, false, "" },
    };

    editor.set_body_renderer([&widgets](NodeId id, ImDrawList* draw_list,
                                        ImVec2 const& body_min, ImVec2 const& body_max,
                                        float zoom)
    {
        auto it = widgets.find(id.v);
        if (it == widgets.end() or it->second.empty())
        {
            return;
        }
        auto& fields = it->second;

        // Stack fields top-down. Labels scale with zoom and always
        // render where they fit -- widgets are fixed pixel size and
        // can only render when there's enough vertical room left
        // below their label. When a widget can't fit, the row
        // collapses to label-only so the user still sees what the
        // field is.
        constexpr float widget_h  = 22.0f;
        constexpr float pad_x     = 6.0f;
        constexpr float pad_y     = 3.0f;
        constexpr float label_gap = 2.0f;
        constexpr float field_gap = 4.0f;

        ImFont* const font      = ImGui::GetFont();
        float   const font_size = ImGui::GetFontSize() * zoom;
        float   const widget_w  = (body_max.x - body_min.x) - 2.0f * pad_x;

        ImGui::PushID(int(id.v));
        float y = body_min.y + pad_y;
        for (std::size_t k = 0; k < fields.size(); ++k)
        {
            Field& f = fields[k];

            float const y_label = y;
            if (y_label + font_size > body_max.y)
            {
                break;
            }
            ImVec2 const label_pos{ body_min.x + pad_x, y_label };
            draw_list->AddText(font, font_size, label_pos,
                               IM_COL32(0xC0, 0xC0, 0xC0, 0xFF),
                               f.label.data(), f.label.data() + f.label.size());

            float const y_widget       = y_label + font_size + label_gap;
            bool  const widget_fits    = y_widget + widget_h <= body_max.y;
            float       advance_to     = y_label + font_size + field_gap;

            if (widget_fits)
            {
                ImVec2 const widget_pos{ body_min.x + pad_x, y_widget };
                ImGui::PushID(int(k));
                ImGui::SetCursorScreenPos(widget_pos);
                ImGui::SetNextItemWidth(widget_w);
                switch (f.kind)
                {
                    case WidgetKind::Float:
                    {
                        ImGui::InputFloat("##v", &f.f, 0.0f, 0.0f, "%.3f");
                        break;
                    }
                    case WidgetKind::Int:
                    {
                        ImGui::InputInt("##v", &f.i);
                        break;
                    }
                    case WidgetKind::Bool:
                    {
                        ImGui::Checkbox("##v", &f.b);
                        break;
                    }
                    case WidgetKind::Text:
                    {
                        char buf[64];
                        std::strncpy(buf, f.txt.c_str(), sizeof(buf) - 1);
                        buf[sizeof(buf) - 1] = '\0';
                        if (ImGui::InputText("##v", buf, sizeof(buf)))
                        {
                            f.txt = buf;
                        }
                        break;
                    }
                }
                ImGui::PopID();
                advance_to = y_widget + widget_h + field_gap;
            }
            y = advance_to;
        }
        ImGui::PopID();
    });

    editor.set_context_menu([](NodeId id, ImVec2 const& canvas_pos)
    {
        if (id == invalid_node_id)
        {
            ImGui::Text("Empty area at (%.0f, %.0f)", canvas_pos.x, canvas_pos.y);
        }
        else
        {
            ImGui::Text("Node %llu", (unsigned long long)id.v);
            ImGui::Separator();
            if (ImGui::MenuItem("Print"))
            {
                std::printf("ctx: node %llu at (%.0f, %.0f)\n",
                            (unsigned long long)id.v, canvas_pos.x, canvas_pos.y);
            }
        }
    });

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
        auto const events = editor.consume_events();

        bool any_mutation = false;
        for (auto const& ev : events)
        {
            if (DemoGraph::mutates(ev.kind))
            {
                any_mutation = true;
                break;
            }
        }
        if (any_mutation)
        {
            graph.take_snapshot();
        }
        for (auto const& ev : events)
        {
            if (ev.kind == EventKind::UndoRequested)
            {
                if (not graph.undo())
                {
                    std::printf("undo: stack empty\n");
                }
            }
            else if (ev.kind == EventKind::RedoRequested)
            {
                if (not graph.redo())
                {
                    std::printf("redo: stack empty\n");
                }
            }
            else
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
