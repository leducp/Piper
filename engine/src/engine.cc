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

#include "io_block.h"

namespace piper::engine
{
    struct Engine::Impl
    {
        std::unordered_map<piper::NodeId, Step::IoBlock>  blocks_;
        std::vector<std::string>                          stage_names_;
        std::unordered_map<std::string, std::size_t>      stage_to_index_;
        std::vector<std::vector<piper::NodeId>>           per_stage_order_;
        bool                                              ok_{false};
    };

    Engine::Engine()
        : impl_{std::make_unique<Impl>()}
    {
    }

    Engine::~Engine() = default;
    Engine::Engine(Engine&&) noexcept            = default;
    Engine& Engine::operator=(Engine&&) noexcept = default;

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

        impl_->blocks_.clear();
        impl_->stage_names_.clear();
        impl_->stage_to_index_.clear();
        impl_->per_stage_order_.clear();
        impl_->ok_ = false;

        // ---- Snapshot stages ----
        impl_->stage_names_.reserve(graph.stages().size());
        for (auto const& s : graph.stages())
        {
            impl_->stage_to_index_[s.name] = impl_->stage_names_.size();
            impl_->stage_names_.push_back(s.name);
        }
        impl_->per_stage_order_.resize(impl_->stage_names_.size());

        bool has_error = false;

        // ---- Construct steps and call declare_io ----
        impl_->blocks_.reserve(graph.nodes().size());
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

            auto [it, inserted] = impl_->blocks_.emplace(node.id, Step::IoBlock{});
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
            auto src_it = impl_->blocks_.find(link.from.node);
            auto dst_it = impl_->blocks_.find(link.to.node);
            if (src_it == impl_->blocks_.end() or dst_it == impl_->blocks_.end())
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

        // ---- Active stages per step ----
        for (auto const& node : graph.nodes())
        {
            auto block_it = impl_->blocks_.find(node.id);
            if (block_it == impl_->blocks_.end())
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
                auto sit = impl_->stage_to_index_.find(s);
                if (sit == impl_->stage_to_index_.end())
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
        for (std::size_t s = 0; s < impl_->stage_names_.size(); ++s)
        {
            std::unordered_set<piper::NodeId> in_subgraph;
            for (auto const& [id, block] : impl_->blocks_)
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
                std::string   message = "cycle detected in stage '" + impl_->stage_names_[s] + "'";
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

            impl_->per_stage_order_[s] = std::move(order);
        }

        impl_->ok_ = not has_error;
        result.ok = impl_->ok_;
        return result;
    }

    void Engine::tick(Stage current)
    {
        if (not impl_->ok_)
        {
            return;
        }
        auto it = impl_->stage_to_index_.find(std::string(current));
        if (it == impl_->stage_to_index_.end())
        {
            return;
        }
        auto const& order = impl_->per_stage_order_[it->second];
        for (auto id : order)
        {
            auto bit = impl_->blocks_.find(id);
            if (bit == impl_->blocks_.end())
            {
                continue;
            }
            bit->second.step->compute(current);
        }
    }

    void Engine::tick_all_stages()
    {
        if (not impl_->ok_)
        {
            return;
        }
        for (auto const& name : impl_->stage_names_)
        {
            tick(Stage{name});
        }
    }

    Step* Engine::step_for(piper::NodeId id)
    {
        auto it = impl_->blocks_.find(id);
        if (it == impl_->blocks_.end())
        {
            return nullptr;
        }
        return it->second.step.get();
    }

    std::vector<Stage> Engine::stages() const
    {
        std::vector<Stage> out;
        out.reserve(impl_->stage_names_.size());
        for (auto const& s : impl_->stage_names_)
        {
            out.push_back(Stage{s});
        }
        return out;
    }
}
