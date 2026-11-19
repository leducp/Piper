#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>

#include <piper/builtin_nodes.h>
#include <piper/graph.h>
#include <piper/registry.h>
#include <piper/serialize_v2.h>
#include <piper/engine/builtin_steps.h>
#include <piper/engine/engine.h>
#include <piper/engine/registry.h>
#include <piper/engine/step.h>

// --8<-- [start:node_type]
piper::NodeType gain_type()
{
    using Role = piper::AttributeSpec::Role;
    piper::NodeType type;
    type.type = "guide_gain";
    type.category = "math";
    type.help = "Scale a signal or pass it through.";
    // These names appear on the canvas and in the Inspector.
    type.attributes = {
        {"in", "float", Role::Input, ""},
        {"factor", "float", Role::Member, "2.0"},
        {"out", "float", Role::Output, ""},
    };
    type.modes = {"scale", "passthrough"};
    return type;
}
// --8<-- [end:node_type]

// --8<-- [start:step]
class Gain final : public piper::engine::Step
{
public:
    void declare_io() override
    {
        // Match the node type's pin names and let Piper read out of result_.
        declare_input<float>("in");
        declare_output<float>("out", result_);
    }

    void on_mode_change(piper::engine::HashedString mode) override
    {
        // Piper has already picked the default or the override for this mode.
        auto const& value = member("factor");
        std::size_t parsed = 0;
        float const factor = std::stof(value, &parsed);
        if (parsed != value.size())
        {
            throw std::invalid_argument("factor must be a float");
        }
        factor_ = factor;
        if (mode == "passthrough")
        {
            // A factor of one sends the input straight through.
            factor_ = 1.0f;
        }
    }

    void compute(piper::engine::HashedString) override
    {
        result_ = input<float>("in") * factor_;
    }

private:
    float result_{};
    float factor_{};
};
// --8<-- [end:step]

void write_file(std::string const& path, std::string const& content)
{
    std::ofstream file(path);
    file.exceptions(std::ios::failbit | std::ios::badbit);
    file << content;
    file.close();
}

piper::Graph make_demo_graph(piper::NodeRegistry const& nodes, piper::NodeType const& type)
{
    piper::Graph graph;
    graph.add_stage({"control"});
    // Use built-in nodes to bring a value in from the host and send the result back.
    auto const source = graph.add_node(*nodes.find("external_input<float>"),
                                      "source", "control", {0, 0});
    auto const gain = graph.add_node(type, "gain", "control", {230, 0});
    auto const sink = graph.add_node(*nodes.find("external_output<float>"),
                                    "sink", "control", {460, 0});
    graph.set_attr_value(source, "value", "signal");
    graph.set_attr_value(sink, "value", "result");
    graph.add_link({source, "out"}, {gain, "in"}, "float");
    graph.add_link({gain, "out"}, {sink, "in"}, "float");

    // Add a mode to this node so one profile can use a different factor.
    graph.add_node_mode(gain, "boost", 2);
    graph.set_mode_overload(gain, "factor", "boost", "4.0");
    graph.replace_profiles({
        {"normal", {{source, "enable"}, {gain, "scale"}, {sink, "enable"}}},
        {"high_gain", {{source, "enable"}, {gain, "boost"}, {sink, "enable"}}},
        {"monitor", {{source, "enable"}, {gain, "passthrough"}, {sink, "enable"}}},
    }, "normal");
    return graph;
}

int main(int argc, char** argv)
try
{
    auto const type = gain_type();
    // --8<-- [start:register]
    piper::NodeRegistry nodes;
    nodes.add("guide", type);
    piper::engine::StepRegistry steps;
    // Use the same type key so the engine knows which class to create.
    steps.add(type.type, [] { return std::make_shared<Gain>(); });
    // --8<-- [end:register]

    // Export the custom type for the editor, which already has the built-ins.
    auto const node_pack = piper::v2::serialize_registry(nodes);
    piper::register_builtin_nodes(nodes);
    piper::engine::register_builtin_steps(steps);

    std::string pipeline;
    if (argc < 2)
    {
        // A first run makes the files used in the editor exercise.
        pipeline = piper::v2::serialize(make_demo_graph(nodes, type), "gain demo");
        write_file("gain_nodes.json", node_pack);
        write_file("gain_demo.piper", pipeline);
    }
    else
    {
        std::ifstream file(argv[1]);
        if (!file)
        {
            throw std::runtime_error(std::string{"Could not open "} + argv[1]);
        }
        pipeline.assign(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
    }

    auto const loaded = piper::v2::deserialize(pipeline, nodes);
    if (!loaded.diagnostics.empty())
    {
        for (auto const& diagnostic : loaded.diagnostics)
        {
            std::cerr << diagnostic.message << '\n';
        }
        return 1;
    }

    piper::engine::Engine engine;
    auto const build = engine.build(loaded.graph, steps);
    if (!build.ok)
    {
        for (auto const& diagnostic : build.diagnostics)
        {
            std::cerr << diagnostic.message << '\n';
        }
        return 1;
    }

    auto* signal = engine.input<float>("signal");
    auto const* result = engine.output<float>("result");
    if (signal == nullptr || result == nullptr)
    {
        throw std::runtime_error("Expected float ports named signal and result");
    }

    // Keep the input fixed so the three outputs show what the profile changes.
    *signal = 3.0f;
    for (auto const& profile : loaded.graph.profiles())
    {
        if (!engine.set_profile(profile.name))
        {
            throw std::runtime_error("Unknown profile " + profile.name);
        }
        engine.play();
        std::cout << profile.name << " -> " << *result << '\n';
    }
}
catch (std::exception const& error)
{
    std::cerr << "gain_demo: " << error.what() << '\n';
    return 1;
}
