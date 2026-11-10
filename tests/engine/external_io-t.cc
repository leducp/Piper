#include <gtest/gtest.h>

#include <memory>

#include "piper/builtin_nodes.h"
#include "piper/graph.h"
#include "piper/node_type.h"
#include "piper/registry.h"
#include "piper/stage.h"

#include "piper/engine/builtin_steps.h"
#include "piper/engine/engine.h"
#include "piper/engine/registry.h"

using piper::AttributeSpec;
using piper::Graph;
using piper::Link;
using piper::NodeRegistry;
using piper::NodeType;
using piper::PinRef;
using piper::Point;
using piper::engine::BuildDiagnostic;
using piper::engine::Engine;
using piper::engine::StepRegistry;

namespace
{
    Graph build_external_passthrough()
    {
        NodeRegistry nr;
        piper::register_builtin_nodes(nr);

        Graph g;
        g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

        auto const* in_t  = nr.find("external_input<float>");
        auto const* out_t = nr.find("external_output<float>");
        auto const* lp_t  = nr.find("low_pass<float>");

        auto in_id  = g.add_node(*in_t,  "ipc_in",  "control", Point{ 0.0f, 0.0f });
        auto lp_id  = g.add_node(*lp_t,  "filter",  "control", Point{ 1.0f, 0.0f });
        auto out_id = g.add_node(*out_t, "ipc_out", "control", Point{ 2.0f, 0.0f });

        g.set_attr_value(in_id,  "value",  "target");
        g.set_attr_value(lp_id,  "cutoff", "100.0");
        g.set_attr_value(out_id, "value",  "measured");

        g.add_link(PinRef{ in_id,  "out" }, PinRef{ lp_id,  "in" }, "float");
        g.add_link(PinRef{ lp_id,  "out" }, PinRef{ out_id, "in" }, "float");

        return g;
    }

    Graph build_external_passthrough_double()
    {
        NodeRegistry nr;
        piper::register_builtin_nodes(nr);

        Graph g;
        g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

        auto const* in_t  = nr.find("external_input<double>");
        auto const* out_t = nr.find("external_output<double>");
        auto const* lp_t  = nr.find("low_pass<double>");

        auto in_id  = g.add_node(*in_t,  "ipc_in",  "control", Point{ 0.0f, 0.0f });
        auto lp_id  = g.add_node(*lp_t,  "filter",  "control", Point{ 1.0f, 0.0f });
        auto out_id = g.add_node(*out_t, "ipc_out", "control", Point{ 2.0f, 0.0f });

        g.set_attr_value(in_id,  "value",  "target");
        g.set_attr_value(lp_id,  "cutoff", "100.0");
        g.set_attr_value(out_id, "value",  "measured");

        g.add_link(PinRef{ in_id,  "out" }, PinRef{ lp_id,  "in" }, "double");
        g.add_link(PinRef{ lp_id,  "out" }, PinRef{ out_id, "in" }, "double");

        return g;
    }

    class Device final : public piper::engine::Step
    {
    public:
        void declare_io() override
        {
            declare_external_input<float>("cmd", cmd_);
            declare_external_input<int32_t>("gain", gain_);
            declare_external_output<float>("scaled", scaled_);
            declare_external_output<int32_t>("ticks", ticks_);
        }

        void compute(piper::engine::Stage) override
        {
            scaled_ = cmd_ * static_cast<float>(gain_);
            ++ticks_;
        }

        float const* cmd_addr() const { return &cmd_; }

    private:
        float   cmd_{0.0f};
        int32_t gain_{0};
        float   scaled_{0.0f};
        int32_t ticks_{0};
    };

    NodeType device_meta()
    {
        NodeType nt;
        nt.type       = "device";
        nt.attributes = {
            { "cmd",    "float",   AttributeSpec::Role::ExternalInput,  "" },
            { "gain",   "int32_t", AttributeSpec::Role::ExternalInput,  "" },
            { "scaled", "float",   AttributeSpec::Role::ExternalOutput, "" },
            { "ticks",  "int32_t", AttributeSpec::Role::ExternalOutput, "" },
        };
        return nt;
    }

    // The graph expects an external attribute, but the step declares a pin.
    class WrongKindStep final : public piper::engine::Step
    {
    public:
        void declare_io() override { declare_output<float>("value", v_); }
        void compute(piper::engine::Stage) override {}

    private:
        float v_{0.0f};
    };

    class WrongRoleStep final : public piper::engine::Step
    {
    public:
        void declare_io() override { declare_external_input<float>("value", v_); }
        void compute(piper::engine::Stage) override {}

    private:
        float v_{0.0f};
    };

    class SilentStep final : public piper::engine::Step
    {
    public:
        void compute(piper::engine::Stage) override {}
    };

    NodeType single_attr_meta(std::string type, AttributeSpec::Role role)
    {
        NodeType nt;
        nt.type       = std::move(type);
        nt.attributes = { { "value", "float", role, "" } };
        return nt;
    }

    bool has_conflict(Engine::BuildResult const& res, std::string const& needle)
    {
        for (auto const& d : res.diagnostics)
        {
            if (d.kind == BuildDiagnostic::Kind::ExternalIoConflict
                and d.message.find(needle) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    std::string first_message(Engine::BuildResult const& res)
    {
        if (res.diagnostics.empty())
        {
            return {};
        }
        return res.diagnostics.front().message;
    }

    template<typename S>
    void register_step(StepRegistry& sr, std::string const& type)
    {
        sr.add(type, [] { return std::make_shared<S>(); });
    }
}


TEST(EngineExternalIO, SetInputFlowsThroughPipelineToOutput)
{
    Graph g = build_external_passthrough();

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok) << first_message(res);

    float*       target   = e.input<float>("target");
    float const* measured = e.output<float>("measured");
    ASSERT_NE(target, nullptr);
    ASSERT_NE(measured, nullptr);

    *target = 1.0f;
    for (int i = 0; i < 1000; ++i)
    {
        e.play();
    }
    EXPECT_NEAR(*measured, 1.0f, 1e-3f);

    *target = -2.5f;
    for (int i = 0; i < 1000; ++i)
    {
        e.play();
    }
    EXPECT_NEAR(*measured, -2.5f, 1e-3f);
}

TEST(EngineExternalIO, DoubleSetInputFlowsThroughPipelineToOutput)
{
    Graph g = build_external_passthrough_double();

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok) << first_message(res);

    double*       target   = e.input<double>("target");
    double const* measured = e.output<double>("measured");
    ASSERT_NE(target, nullptr);
    ASSERT_NE(measured, nullptr);

    *target = 1.0;
    for (int i = 0; i < 1000; ++i)
    {
        e.play();
    }
    EXPECT_NEAR(*measured, 1.0, 1e-3);
}

TEST(EngineExternalIO, UnknownNameOrWrongTypeReturnsNullptr)
{
    Graph g = build_external_passthrough();
    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);
    Engine e;
    ASSERT_TRUE(e.build(g, sr).ok);

    EXPECT_EQ(e.input<float>("does_not_exist"),  nullptr);
    EXPECT_EQ(e.output<float>("does_not_exist"), nullptr);
    EXPECT_EQ(e.input<int32_t>("target"),        nullptr);   // wrong type
    EXPECT_EQ(e.input<double>("target"),         nullptr);   // wrong width
    EXPECT_EQ(e.output<double>("measured"),      nullptr);
    // Directions are separate namespaces.
    EXPECT_EQ(e.output<float>("target"),         nullptr);
    EXPECT_EQ(e.input<float>("measured"),        nullptr);
}

TEST(EngineExternalIO, StepWithSeveralExternalInputsAndOutputs)
{
    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto id = g.add_node(device_meta(), "dev", "control", Point{});
    g.set_attr_value(id, "cmd",    "dev.cmd");
    g.set_attr_value(id, "gain",   "dev.gain");
    g.set_attr_value(id, "scaled", "dev.scaled");
    g.set_attr_value(id, "ticks",  "dev.ticks");

    StepRegistry sr;
    register_step<Device>(sr, "device");

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok) << first_message(res);

    float*         cmd    = e.input<float>("dev.cmd");
    int32_t*       gain   = e.input<int32_t>("dev.gain");
    float const*   scaled = e.output<float>("dev.scaled");
    int32_t const* ticks  = e.output<int32_t>("dev.ticks");
    ASSERT_NE(cmd, nullptr);
    ASSERT_NE(gain, nullptr);
    ASSERT_NE(scaled, nullptr);
    ASSERT_NE(ticks, nullptr);

    // The host writes directly to the step's field.
    auto const* dev = dynamic_cast<Device const*>(e.step_for(id));
    ASSERT_NE(dev, nullptr);
    EXPECT_EQ(cmd, dev->cmd_addr());

    *cmd  = 1.5f;
    *gain = 4;
    e.play();
    EXPECT_FLOAT_EQ(*scaled, 6.0f);
    EXPECT_EQ(*ticks, 1);

    *gain = -2;
    e.play();
    EXPECT_FLOAT_EQ(*scaled, -3.0f);
    EXPECT_EQ(*ticks, 2);
}

TEST(EngineExternalIO, InputAndOutputMayShareAName)
{
    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto id = g.add_node(device_meta(), "dev", "control", Point{});
    g.set_attr_value(id, "cmd",    "x");
    g.set_attr_value(id, "gain",   "gain");
    g.set_attr_value(id, "scaled", "x");

    StepRegistry sr;
    register_step<Device>(sr, "device");

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok) << first_message(res);
    EXPECT_NE(e.input<float>("x"), nullptr);
    EXPECT_NE(e.output<float>("x"), nullptr);
}

TEST(EngineExternalIO, EmptyOutputNameIsNotExportedButStillTicks)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });

    auto const* cf    = nr.find("constant<float>");
    auto const* out_t = nr.find("external_output<float>");
    auto src_id  = g.add_node(*cf,    "src",  "control", Point{ 0.0f, 0.0f });
    auto sink_id = g.add_node(*out_t, "sink", "control", Point{ 1.0f, 0.0f });
    g.set_attr_value(src_id, "value", "2.0");
    g.add_link(PinRef{ src_id, "out" }, PinRef{ sink_id, "in" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok) << first_message(res);

    e.tick("control");

    EXPECT_EQ(e.output<float>(""), nullptr);
    auto const* by_id = e.step_for(sink_id);
    ASSERT_NE(by_id, nullptr);
    EXPECT_FLOAT_EQ(by_id->input<float>("in"), 2.0f);
}

TEST(EngineExternalIO, EmptyInputNameIsNotExportedButStillTicks)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto in_id   = g.add_node(*nr.find("external_input<float>"),  "in",   "control", Point{});
    auto sink_id = g.add_node(*nr.find("external_output<float>"), "sink", "control",
                              Point{ 1.0f, 0.0f });
    g.set_attr_value(sink_id, "value", "measured");
    g.add_link(PinRef{ in_id, "out" }, PinRef{ sink_id, "in" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    ASSERT_TRUE(res.ok) << first_message(res);
    EXPECT_EQ(e.input<float>(""), nullptr);

    // The unnamed input keeps its default value, like an unwired pin.
    e.tick("control");
    auto const* measured = e.output<float>("measured");
    ASSERT_NE(measured, nullptr);
    EXPECT_FLOAT_EQ(*measured, 0.0f);
}

TEST(EngineExternalIO, DuplicateInputNameFailsBuild)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto const* in_t = nr.find("external_input<double>");
    auto a_id = g.add_node(*in_t, "in_a", "control", Point{ 0.0f, 0.0f });
    auto b_id = g.add_node(*in_t, "in_b", "control", Point{ 1.0f, 0.0f });
    g.set_attr_value(a_id, "value", "target");
    g.set_attr_value(b_id, "value", "target");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    EXPECT_TRUE(has_conflict(res, "duplicate external input name 'target'"));
}

TEST(EngineExternalIO, DuplicateNameAcrossTypesFailsBuild)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto a_id = g.add_node(*nr.find("external_input<float>"),  "a", "control", Point{});
    auto b_id = g.add_node(*nr.find("external_input<double>"), "b", "control", Point{});
    g.set_attr_value(a_id, "value", "target");
    g.set_attr_value(b_id, "value", "target");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    EXPECT_TRUE(has_conflict(res, "duplicate external input name 'target'"));
}

TEST(EngineExternalIO, DuplicateOutputNameFailsBuild)
{
    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto id = g.add_node(device_meta(), "dev", "control", Point{});
    g.set_attr_value(id, "cmd",    "cmd");
    g.set_attr_value(id, "gain",   "gain");
    g.set_attr_value(id, "scaled", "out");
    g.set_attr_value(id, "ticks",  "out");

    StepRegistry sr;
    register_step<Device>(sr, "device");

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    EXPECT_TRUE(has_conflict(res, "duplicate external output name 'out'"));
}

TEST(EngineExternalIO, AttributeNotDeclaredByStepFailsBuild)
{
    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto id = g.add_node(single_attr_meta("silent", AttributeSpec::Role::ExternalInput),
                         "s", "control", Point{});
    g.set_attr_value(id, "value", "v");

    StepRegistry sr;
    register_step<SilentStep>(sr, "silent");

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    EXPECT_TRUE(has_conflict(res, "is not declared by the step"));
}

TEST(EngineExternalIO, ExternalAttributeDeclaredAsPinFailsBuild)
{
    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto id = g.add_node(single_attr_meta("wrong_kind", AttributeSpec::Role::ExternalOutput),
                         "w", "control", Point{});
    g.set_attr_value(id, "value", "v");

    StepRegistry sr;
    register_step<WrongKindStep>(sr, "wrong_kind");

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    EXPECT_TRUE(has_conflict(res, "declared by the step as a linkable pin"));
    EXPECT_EQ(e.output<float>("v"), nullptr);
}

TEST(EngineExternalIO, ExternalDeclarationOnNonExternalAttributeFailsBuild)
{
    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    g.add_node(single_attr_meta("wrong_role", AttributeSpec::Role::Member),
               "w", "control", Point{});

    StepRegistry sr;
    register_step<WrongRoleStep>(sr, "wrong_role");

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    EXPECT_TRUE(has_conflict(res, "step declares external input 'value'"));
}

TEST(EngineExternalIO, DataTypeMismatchBetweenGraphAndStepFailsBuild)
{
    // The graph says double, the step exposes float: the build fails
    // instead of handing out a nullptr for input<double>.
    NodeType nt = single_attr_meta("wrong_role", AttributeSpec::Role::ExternalInput);
    nt.attributes[0].data_type = "double";

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto id = g.add_node(nt, "w", "control", Point{});
    g.set_attr_value(id, "value", "command");

    StepRegistry sr;
    register_step<WrongRoleStep>(sr, "wrong_role");

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
    EXPECT_TRUE(has_conflict(res, "is declared as 'double' in the graph, but the step exposes 'float'"));
    EXPECT_EQ(e.input<float>("command"), nullptr);
}

TEST(EngineExternalIO, HandBuiltLinkToExternalAttributeFailsBuild)
{
    NodeRegistry nr;
    piper::register_builtin_nodes(nr);

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto src_id = g.add_node(*nr.find("constant<float>"),       "src", "control", Point{});
    auto in_id  = g.add_node(*nr.find("external_input<float>"), "in",  "control", Point{});
    g.set_attr_value(in_id, "value", "target");

    // Bypass editor validation to check that the engine rejects this link.
    g.add_link(PinRef{ src_id, "out" }, PinRef{ in_id, "value" }, "float");

    StepRegistry sr;
    piper::engine::register_builtin_steps(sr);

    Engine e;
    auto res = e.build(g, sr);
    EXPECT_FALSE(res.ok);
}
