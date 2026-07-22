#include <gtest/gtest.h>

#include <memory>

#include "piper/attribute.h"
#include "piper/graph.h"
#include "piper/node_type.h"
#include "piper/stage.h"

#include "piper/engine/engine.h"
#include "piper/engine/registry.h"
#include "piper/engine/step.h"

using piper::AttributeSpec;
using piper::Graph;
using piper::NodeType;
using piper::PinRef;
using piper::Point;
using piper::engine::BuildDiagnostic;
using piper::engine::Engine;
using piper::engine::Step;
using piper::engine::StepRegistry;

namespace piper_engine_test
{
    // Reads an OPTIONAL "command" input, falling back to a member when
    // the pin is unwired -- the read-only / monitor-pipeline pattern the
    // downstream project relies on. build() must NOT flag the unwired
    // optional pin, and compute() must not throw because it guards the
    // read with has_input().
    class OptionalCommandStep final : public Step
    {
    public:
        void declare_io() override
        {
            declare_output<double>("out", out_);
            declare_input<double>("command", /*optional=*/true);
        }

        void compute(piper::engine::Stage) override
        {
            if (has_input("command"))
            {
                out_ = input<double>("command");
            }
            else
            {
                out_ = fallback_;
            }
        }

        double out() const { return out_; }

    private:
        double out_{0.0};
        double fallback_{-1.0};
    };

    // Same shape but the "command" input is REQUIRED (declared without the
    // optional flag). An unwired required input must fail the build.
    class RequiredCommandStep final : public Step
    {
    public:
        void declare_io() override
        {
            declare_output<double>("out", out_);
            declare_input<double>("command");
        }

        void compute(piper::engine::Stage) override
        {
            out_ = input<double>("command");
        }

    private:
        double out_{0.0};
    };

    NodeType command_meta(char const* type, bool optional)
    {
        NodeType nt;
        nt.type       = type;
        nt.category   = "test";
        nt.attributes = {
            { "out",     "double", AttributeSpec::Role::Output, "" },
            { "command", "double", AttributeSpec::Role::Input,  "", false, optional },
        };
        return nt;
    }
}

// An optional command input left unwired builds successfully, and the step
// reads it safely through has_input() (falling back to its member).
TEST(EngineOptionalInput, OptionalUnwiredInputBuildsAndReadsSafely)
{
    using namespace piper_engine_test;

    auto step = std::make_shared<OptionalCommandStep>();

    StepRegistry sr;
    sr.add("optional_command", [step] { return step; });

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    auto id = g.add_node(command_meta("optional_command", true), "dev", "control", Point{ 0.f, 0.f });
    (void)id;

    Engine e;
    auto res = e.build(g, sr);

    ASSERT_TRUE(res.ok) << (res.diagnostics.empty()
                                ? std::string{"no diagnostics"}
                                : res.diagnostics.front().message);
    EXPECT_TRUE(res.diagnostics.empty());

    // compute() must not throw on the unwired optional pin; it falls back.
    e.play();
    EXPECT_DOUBLE_EQ(step->out(), -1.0);
}

// The sibling case: a required (non-optional) input left unwired fails the
// build with an UnresolvedInput diagnostic naming the pin.
TEST(EngineOptionalInput, RequiredUnwiredInputFailsBuild)
{
    using namespace piper_engine_test;

    auto step = std::make_shared<RequiredCommandStep>();

    StepRegistry sr;
    sr.add("required_command", [step] { return step; });

    Graph g;
    g.add_stage(piper::Stage{ "control", 0xFFFFFFFFu });
    g.add_node(command_meta("required_command", false), "dev", "control", Point{ 0.f, 0.f });

    Engine e;
    auto res = e.build(g, sr);

    EXPECT_FALSE(res.ok);
    bool flagged_command = false;
    for (auto const& d : res.diagnostics)
    {
        if (d.kind == BuildDiagnostic::Kind::UnresolvedInput and d.attr_name == "command")
        {
            flagged_command = true;
        }
    }
    EXPECT_TRUE(flagged_command) << "required 'command' should fail the build";
}
