#include "piper/engine/engine.h"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <exception>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "piper/attribute.h"
#include "piper/link.h"
#include "piper/node.h"
#include "piper/stage.h"

#include "piper/engine/external_io.h"

namespace piper::engine
{
    BuildDiagnostic make_build_diagnostic(BuildDiagnosticKind kind,
                                          std::string         message,
                                          piper::NodeId       node_id   = piper::invalid_node_id,
                                          std::string         attr_name = {},
                                          piper::LinkId       link_id   = piper::invalid_link_id)
    {
        BuildDiagnostic d;
        d.kind      = kind;
        d.message   = std::move(message);
        d.node_id   = node_id;
        d.attr_name = std::move(attr_name);
        d.link_id   = link_id;
        return d;
    }

    Engine::BuildResult Engine::build(piper::Graph const& graph,
                                      StepRegistry const& step_reg)
    {
        BuildResult result;
        result.ok = false;

        blocks_.clear();
        stage_names_.clear();
        stage_views_.clear();
        stage_to_index_.clear();
        per_stage_order_.clear();
        input_float_.clear();
        input_int_.clear();
        output_float_.clear();
        output_int_.clear();
        ok_ = false;

        // ---- Snapshot stages ----
        // stage_names_ is reserved before the loop so the strings keep
        // stable addresses; stage_views_ relies on that.
        stage_names_.reserve(graph.stages().size());
        stage_views_.reserve(graph.stages().size());
        for (auto const& s : graph.stages())
        {
            stage_to_index_[s.name] = stage_names_.size();
            stage_names_.push_back(s.name);
            stage_views_.push_back(Stage{ stage_names_.back() });
        }
        per_stage_order_.resize(stage_names_.size());

        bool has_error = false;

        // ---- Construct steps and call declare_io ----
        blocks_.reserve(graph.nodes().size());
        for (auto const& node : graph.nodes())
        {
            auto const* factory = step_reg.find(node.type);
            if (factory == nullptr)
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnosticKind::UnknownStepFactory,
                              "no factory registered for type '" + node.type + "'",
                              node.id));
                has_error = true;
                continue;
            }

            auto step_ptr = (*factory)();
            if (not step_ptr)
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnosticKind::UnknownStepFactory,
                              "factory for type '" + node.type + "' returned null",
                              node.id));
                has_error = true;
                continue;
            }

            auto [it, inserted] = blocks_.emplace(node.id, IoBlock{});
            auto& block         = it->second;
            block.node_id       = node.id;
            block.step          = std::move(step_ptr);

            for (auto const& attr : node.attrs)
            {
                if (attr.role == piper::AttributeSpec::Role::Member)
                {
                    block.members.emplace(attr.name, attr.value);
                }
            }

            block.step->io_ = &block;
            try
            {
                block.step->declare_io();
            }
            catch (std::exception const& e)
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnosticKind::StepDeclareIoFailed,
                              std::string{"declare_io threw: "} + e.what(),
                              node.id));
                has_error = true;
            }
        }

        // ---- Wire links ----
        for (auto const& link : graph.links())
        {
            auto src_it = blocks_.find(link.from.node);
            auto dst_it = blocks_.find(link.to.node);
            if (src_it == blocks_.end() or dst_it == blocks_.end())
            {
                continue;
            }

            auto& src_block = src_it->second;
            auto& dst_block = dst_it->second;

            auto out_it = src_block.outputs.find(link.from.attr);
            if (out_it == src_block.outputs.end())
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnosticKind::UnresolvedInput,
                              "link source '" + link.from.attr + "' is not a published output",
                              link.from.node, link.from.attr, link.id));
                has_error = true;
                continue;
            }

            auto in_it = dst_block.input_decls.find(link.to.attr);
            if (in_it == dst_block.input_decls.end())
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnosticKind::UnresolvedInput,
                              "link target '" + link.to.attr + "' is not a declared input",
                              link.to.node, link.to.attr, link.id));
                has_error = true;
                continue;
            }

            if (*out_it->second.type != *in_it->second.type)
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnosticKind::TypeMismatchAtLink,
                              std::string{"producer type '"} + out_it->second.type->name()
                                  + "' != consumer type '" + in_it->second.type->name() + "'",
                              link.to.node, link.to.attr, link.id));
                has_error = true;
                continue;
            }

            dst_block.inputs[link.to.attr] = out_it->second.ref_any;
        }

        // ---- Index external_input / external_output nodes by name ----
        // Hot-path accessors (Engine::input<T>/output<T>) resolve via
        // these maps so HAL code does not search per tick.
        for (auto const& node : graph.nodes())
        {
            bool const is_ext = node.type == "external_input<float>"
                             or node.type == "external_input<int>"
                             or node.type == "external_output<float>"
                             or node.type == "external_output<int>";
            if (not is_ext)
            {
                continue;
            }

            auto block_it = blocks_.find(node.id);
            if (block_it == blocks_.end())
            {
                continue;
            }

            std::string name;
            for (auto const& attr : node.attrs)
            {
                if (attr.role == piper::AttributeSpec::Role::Member and attr.name == "name")
                {
                    name = attr.value;
                    break;
                }
            }
            // Empty name skips HAL-side indexing. The node still ticks
            // normally and is accessible via step_for(node.id); it
            // just isn't reachable through Engine::input/output(name).
            if (name.empty())
            {
                continue;
            }

            Step* step = block_it->second.step.get();
            if (node.type == "external_input<float>")
            {
                auto* typed = static_cast<step::Input<float>*>(step);
                if (not input_float_.emplace(name, typed).second)
                {
                    result.diagnostics.push_back(
                        make_build_diagnostic(BuildDiagnosticKind::UnresolvedInput,
                                  "duplicate external_input<float> name '" + name + "'",
                                  node.id, "name"));
                    has_error = true;
                }
            }
            else if (node.type == "external_input<int>")
            {
                auto* typed = static_cast<step::Input<int>*>(step);
                if (not input_int_.emplace(name, typed).second)
                {
                    result.diagnostics.push_back(
                        make_build_diagnostic(BuildDiagnosticKind::UnresolvedInput,
                                  "duplicate external_input<int> name '" + name + "'",
                                  node.id, "name"));
                    has_error = true;
                }
            }
            else if (node.type == "external_output<float>")
            {
                auto* typed = static_cast<step::Output<float>*>(step);
                if (not output_float_.emplace(name, typed).second)
                {
                    result.diagnostics.push_back(
                        make_build_diagnostic(BuildDiagnosticKind::UnresolvedInput,
                                  "duplicate external_output<float> name '" + name + "'",
                                  node.id, "name"));
                    has_error = true;
                }
            }
            else
            {
                auto* typed = static_cast<step::Output<int>*>(step);
                if (not output_int_.emplace(name, typed).second)
                {
                    result.diagnostics.push_back(
                        make_build_diagnostic(BuildDiagnosticKind::UnresolvedInput,
                                  "duplicate external_output<int> name '" + name + "'",
                                  node.id, "name"));
                    has_error = true;
                }
            }
        }

        // ---- Active stages per step ----
        for (auto const& node : graph.nodes())
        {
            auto block_it = blocks_.find(node.id);
            if (block_it == blocks_.end())
            {
                continue;
            }
            auto& block = block_it->second;

            std::unordered_set<std::size_t> active;

            auto absorb_stage = [&](std::string const& s, std::string const& attr_name)
            {
                if (s.empty())
                {
                    return;
                }
                auto sit = stage_to_index_.find(s);
                if (sit == stage_to_index_.end())
                {
                    result.diagnostics.push_back(
                        make_build_diagnostic(BuildDiagnosticKind::UnknownStageOnPin,
                                  "stage '" + s + "' is not declared on the graph",
                                  node.id, attr_name));
                    return;
                }
                active.insert(sit->second);
            };

            absorb_stage(node.stage, std::string{});

            for (auto const& attr : node.attrs)
            {
                if (attr.role == piper::AttributeSpec::Role::Input
                    or attr.role == piper::AttributeSpec::Role::Output)
                {
                    for (auto const& s : attr.stages)
                    {
                        absorb_stage(s, attr.name);
                    }
                }
            }

            if (active.empty())
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnosticKind::NodeNeverScheduled,
                              "node has no active stage",
                              node.id));
            }

            block.active_stage_indices.assign(active.begin(), active.end());
            std::sort(block.active_stage_indices.begin(), block.active_stage_indices.end());
        }

        // ---- Per-stage topo sort (Kahn) ----
        for (std::size_t s = 0; s < stage_names_.size(); ++s)
        {
            std::unordered_set<piper::NodeId> in_subgraph;
            for (auto const& [id, block] : blocks_)
            {
                auto const& v = block.active_stage_indices;
                if (std::binary_search(v.begin(), v.end(), s))
                {
                    in_subgraph.insert(id);
                }
            }

            std::unordered_map<piper::NodeId, std::vector<piper::NodeId>> succ;
            std::unordered_map<piper::NodeId, std::size_t>                in_degree;
            for (auto id : in_subgraph)
            {
                in_degree[id] = 0;
            }
            for (auto const& link : graph.links())
            {
                if (in_subgraph.count(link.from.node) == 0
                    or in_subgraph.count(link.to.node) == 0)
                {
                    continue;
                }
                succ[link.from.node].push_back(link.to.node);
                ++in_degree[link.to.node];
            }

            std::deque<piper::NodeId> ready;
            for (auto const& [id, deg] : in_degree)
            {
                if (deg == 0)
                {
                    ready.push_back(id);
                }
            }

            std::vector<piper::NodeId> order;
            order.reserve(in_subgraph.size());
            while (not ready.empty())
            {
                auto id = ready.front();
                ready.pop_front();
                order.push_back(id);
                auto sit = succ.find(id);
                if (sit == succ.end())
                {
                    continue;
                }
                for (auto next : sit->second)
                {
                    auto& d = in_degree[next];
                    --d;
                    if (d == 0)
                    {
                        ready.push_back(next);
                    }
                }
            }

            if (order.size() != in_subgraph.size())
            {
                std::unordered_set<piper::NodeId> remaining(in_subgraph.begin(), in_subgraph.end());
                for (auto id : order)
                {
                    remaining.erase(id);
                }
                piper::LinkId witness = piper::invalid_link_id;
                std::string   message = "cycle detected in stage '" + stage_names_[s] + "'";
                for (auto const& link : graph.links())
                {
                    if (remaining.count(link.from.node) != 0
                        and remaining.count(link.to.node) != 0)
                    {
                        witness = link.id;
                        break;
                    }
                }
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnosticKind::CycleDetected,
                              message,
                              piper::invalid_node_id, std::string{}, witness));
                has_error = true;
            }

            per_stage_order_[s] = std::move(order);
        }

        ok_ = not has_error;
        result.ok = ok_;
        return result;
    }

    void Engine::tick(Stage current)
    {
        if (not ok_)
        {
            return;
        }
        auto it = stage_to_index_.find(std::string(current));
        if (it == stage_to_index_.end())
        {
            return;
        }
        auto const& order = per_stage_order_[it->second];
        for (auto id : order)
        {
            auto bit = blocks_.find(id);
            if (bit == blocks_.end())
            {
                continue;
            }
            bit->second.step->compute(current);
        }
    }

    void Engine::play()
    {
        if (not ok_)
        {
            return;
        }
        for (auto stage : stage_views_)
        {
            tick(stage);
        }
    }

    Step* Engine::step_for(piper::NodeId id)
    {
        auto it = blocks_.find(id);
        if (it == blocks_.end())
        {
            return nullptr;
        }
        return it->second.step.get();
    }

    Step const* Engine::step_for(piper::NodeId id) const
    {
        auto it = blocks_.find(id);
        if (it == blocks_.end())
        {
            return nullptr;
        }
        return it->second.step.get();
    }

    std::vector<Stage> const& Engine::stages() const
    {
        return stage_views_;
    }

    template<>
    step::Input<float>* Engine::input<float>(std::string_view name)
    {
        auto it = input_float_.find(std::string(name));
        if (it == input_float_.end())
        {
            return nullptr;
        }
        return it->second;
    }

    template<>
    step::Input<int>* Engine::input<int>(std::string_view name)
    {
        auto it = input_int_.find(std::string(name));
        if (it == input_int_.end())
        {
            return nullptr;
        }
        return it->second;
    }

    template<>
    step::Output<float> const* Engine::output<float>(std::string_view name) const
    {
        auto it = output_float_.find(std::string(name));
        if (it == output_float_.end())
        {
            return nullptr;
        }
        return it->second;
    }

    template<>
    step::Output<int> const* Engine::output<int>(std::string_view name) const
    {
        auto it = output_int_.find(std::string(name));
        if (it == output_int_.end())
        {
            return nullptr;
        }
        return it->second;
    }
}
