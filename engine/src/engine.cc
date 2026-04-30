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
    BuildDiagnostic make_build_diagnostic(BuildDiagnostic::Event event,
                                          std::string         message,
                                          piper::NodeId       node_id   = piper::invalid_node_id,
                                          std::string         attr_name = {},
                                          piper::LinkId       link_id   = piper::invalid_link_id)
    {
        BuildDiagnostic d;
        d.event     = event;
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
        stage_data_.clear();
        per_stage_dispatch_.clear();
        input_float_.clear();
        input_int_.clear();
        output_float_.clear();
        output_int_.clear();
        ok_ = false;

        // ---- Snapshot stages ----
        // stage_names_ is reserved before the loop so the strings keep
        // stable addresses; stage_data_ holds string_views into them.
        stage_names_.reserve(graph.stages().size());
        stage_data_.reserve(graph.stages().size());
        for (auto const& s : graph.stages())
        {
            stage_names_.push_back(s.name);
            stage_data_.emplace_back(stage_names_.back());  // computes id via hash_stage
        }
        per_stage_dispatch_.resize(stage_names_.size());

        auto stage_index_of = [&](std::string_view name) -> std::size_t
        {
            for (std::size_t i = 0; i < stage_data_.size(); ++i)
            {
                if (stage_data_[i].name == name)
                {
                    return i;
                }
            }
            return stage_data_.size();
        };

        bool has_error = false;

        // ---- Construct steps and call declare_io ----
        blocks_.reserve(graph.nodes().size());
        for (auto const& node : graph.nodes())
        {
            auto const* factory = step_reg.find(node.type);
            if (factory == nullptr)
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnostic::Event::UnknownStepFactory,
                              "no factory registered for type '" + node.type + "'",
                              node.id));
                has_error = true;
                continue;
            }

            auto step_ptr = (*factory)();
            if (not step_ptr)
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnostic::Event::UnknownStepFactory,
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

            block.step->init(block);
            try
            {
                block.step->declare_io();
            }
            catch (std::exception const& e)
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnostic::Event::StepDeclareIoFailed,
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

            auto out_it = src_block.output_slots.find(link.from.attr);
            if (out_it == src_block.output_slots.end())
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnostic::Event::UnresolvedInput,
                              "link source '" + link.from.attr + "' is not a published output",
                              link.from.node, link.from.attr, link.id));
                has_error = true;
                continue;
            }

            auto in_it = dst_block.input_slots.find(link.to.attr);
            if (in_it == dst_block.input_slots.end())
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnostic::Event::UnresolvedInput,
                              "link target '" + link.to.attr + "' is not a declared input",
                              link.to.node, link.to.attr, link.id));
                has_error = true;
                continue;
            }

            if (in_it->second.matches == nullptr
                or not in_it->second.matches(out_it->second.ref_any))
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnostic::Event::TypeMismatchAtLink,
                              "producer / consumer pin types differ on link of data_type '"
                                  + link.data_type + "'",
                              link.to.node, link.to.attr, link.id));
                has_error = true;
                continue;
            }

            dst_block.inputs[link.to.attr] = out_it->second.ref_any;
        }

        // ---- Index external_input / external_output nodes by name ----
        for (auto const& node : graph.nodes())
        {
            bool const is_ext = node.type == "external_input<float>"
                             or node.type == "external_input<int32_t>"
                             or node.type == "external_output<float>"
                             or node.type == "external_output<int32_t>";
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
                        make_build_diagnostic(BuildDiagnostic::Event::UnresolvedInput,
                                  "duplicate external_input<float> name '" + name + "'",
                                  node.id, "name"));
                    has_error = true;
                }
            }
            else if (node.type == "external_input<int32_t>")
            {
                auto* typed = static_cast<step::Input<int32_t>*>(step);
                if (not input_int_.emplace(name, typed).second)
                {
                    result.diagnostics.push_back(
                        make_build_diagnostic(BuildDiagnostic::Event::UnresolvedInput,
                                  "duplicate external_input<int32_t> name '" + name + "'",
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
                        make_build_diagnostic(BuildDiagnostic::Event::UnresolvedInput,
                                  "duplicate external_output<float> name '" + name + "'",
                                  node.id, "name"));
                    has_error = true;
                }
            }
            else
            {
                auto* typed = static_cast<step::Output<int32_t>*>(step);
                if (not output_int_.emplace(name, typed).second)
                {
                    result.diagnostics.push_back(
                        make_build_diagnostic(BuildDiagnostic::Event::UnresolvedInput,
                                  "duplicate external_output<int32_t> name '" + name + "'",
                                  node.id, "name"));
                    has_error = true;
                }
            }
        }

        // ---- Per-stage dispatch + topo sort ----
        // For each stage, collect the (node, slot) pairs whose binding
        // points at it, topo-sort the participating nodes, then
        // materialize the per-(node, slot) dispatch in topo order.
        for (auto const& node : graph.nodes())
        {
            for (auto const& [slot, stage_name] : node.slot_bindings)
            {
                if (stage_index_of(stage_name) == stage_data_.size())
                {
                    result.diagnostics.push_back(
                        make_build_diagnostic(BuildDiagnostic::Event::UnknownStageOnPin,
                                  "node binds slot '" + slot + "' to unknown stage '" + stage_name + "'",
                                  node.id, slot));
                    has_error = true;
                }
            }
        }

        for (uint16_t s = 0; s < stage_data_.size(); ++s)
        {
            std::string_view stage_name = stage_data_[s].name;

            std::unordered_map<piper::NodeId, std::vector<std::string>> dispatch_slots;
            for (auto const& node : graph.nodes())
            {
                if (blocks_.find(node.id) == blocks_.end())
                {
                    continue;
                }
                for (auto const& [slot, st] : node.slot_bindings)
                {
                    if (st == stage_name)
                    {
                        dispatch_slots[node.id].push_back(slot);
                    }
                }
            }

            std::unordered_set<piper::NodeId> in_subgraph;
            for (auto const& [id, _] : dispatch_slots)
            {
                in_subgraph.insert(id);
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
                    make_build_diagnostic(BuildDiagnostic::Event::CycleDetected,
                              message,
                              piper::invalid_node_id, std::string{}, witness));
                has_error = true;
            }

            for (auto id : order)
            {
                for (auto const& slot : dispatch_slots[id])
                {
                    per_stage_dispatch_[s].push_back(
                        DispatchEntry{ id, slot, hash_slot(slot) });
                }
            }
        }

        ok_ = not has_error;
        result.ok = ok_;
        return result;
    }

    void Engine::tick_at(std::size_t idx)
    {
        for (auto const& entry : per_stage_dispatch_[idx])
        {
            auto bit = blocks_.find(entry.node_id);
            if (bit == blocks_.end())
            {
                continue;
            }
            Slot const slot{ entry.slot_name, entry.slot_id };
            bit->second.step->compute(slot);
        }
    }

    void Engine::tick(Stage current)
    {
        if (not ok_)
        {
            return;
        }
        // Hash-id compare: one uint64 per iteration. stage_data_ is
        // typically <10 entries, so linear is faster than a hash map
        // and avoids any allocation.
        for (std::size_t i = 0; i < stage_data_.size(); ++i)
        {
            if (stage_data_[i].id == current.id)
            {
                tick_at(i);
                return;
            }
        }
    }

    void Engine::play()
    {
        if (not ok_)
        {
            return;
        }
        for (std::size_t i = 0; i < stage_data_.size(); ++i)
        {
            tick_at(i);
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
        return stage_data_;
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
    step::Input<int32_t>* Engine::input<int32_t>(std::string_view name)
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
    step::Output<int32_t> const* Engine::output<int32_t>(std::string_view name) const
    {
        auto it = output_int_.find(std::string(name));
        if (it == output_int_.end())
        {
            return nullptr;
        }
        return it->second;
    }
}
