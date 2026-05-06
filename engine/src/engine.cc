#include <algorithm>
#include <any>
#include <deque>
#include <exception>
#include <map>
#include <set>
#include <unordered_set>
#include <utility>

#include "piper/engine/engine.h"

#include "piper/attribute.h"
#include "piper/engine/label_resolver.h"
#include "piper/link.h"
#include "piper/stage.h"

namespace piper::engine
{
    BuildDiagnostic make_build_diagnostic(BuildDiagnostic::Kind kind,
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
        stage_data_.clear();
        per_stage_order_.clear();
        input_float_.clear();
        input_int_.clear();
        output_float_.clear();
        output_int_.clear();
        current_mode_name_.clear();
        current_mode_ = Mode{};
        mode_labels_.clear();
        active_disabled_.clear();
        ok_ = false;

        // ---- Snapshot stages ----
        // stage_names_ is reserved before the loop so the strings keep
        // stable addresses; stage_data_ holds string_views into them.
        stage_names_.reserve(graph.stages().size());
        stage_data_.reserve(graph.stages().size());
        for (auto const& s : graph.stages())
        {
            stage_names_.push_back(s.name);
            stage_data_.emplace_back(stage_names_.back());  // computes id via hash_name
        }
        per_stage_order_.resize(stage_names_.size());

        // Helper: linear scan for a stage name -> index. Used during
        // build only; build is not on the hot path.
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
                    make_build_diagnostic(BuildDiagnostic::Kind::UnknownStepFactory,
                              "no factory registered for type '" + node.type + "'",
                              node.id));
                has_error = true;
                continue;
            }

            auto step_ptr = (*factory)();
            if (not step_ptr)
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnostic::Kind::UnknownStepFactory,
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

            block.current_mode = &current_mode_;
            block.step->init(block);
            try
            {
                block.step->declare_io();
            }
            catch (std::exception const& e)
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnostic::Kind::StepDeclareIoFailed,
                              std::string{"declare_io threw: "} + e.what(),
                              node.id));
                has_error = true;
            }
        }

        std::vector<piper::Link> effective_links =
            resolve_label_clusters(graph, result.diagnostics, has_error);

        // ---- Wire links ----
        for (auto const& link : effective_links)
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
                    make_build_diagnostic(BuildDiagnostic::Kind::UnresolvedInput,
                              "link source '" + link.from.attr + "' is not a published output",
                              link.from.node, link.from.attr, link.id));
                has_error = true;
                continue;
            }

            auto in_it = dst_block.input_slots.find(link.to.attr);
            if (in_it == dst_block.input_slots.end())
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnostic::Kind::UnresolvedInput,
                              "link target '" + link.to.attr + "' is not a declared input",
                              link.to.node, link.to.attr, link.id));
                has_error = true;
                continue;
            }

            if (in_it->second.matches == nullptr
                or not in_it->second.matches(out_it->second.ref_any))
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnostic::Kind::TypeMismatchAtLink,
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
                        make_build_diagnostic(BuildDiagnostic::Kind::UnresolvedInput,
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
                        make_build_diagnostic(BuildDiagnostic::Kind::UnresolvedInput,
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
                        make_build_diagnostic(BuildDiagnostic::Kind::UnresolvedInput,
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
                        make_build_diagnostic(BuildDiagnostic::Kind::UnresolvedInput,
                                  "duplicate external_output<int32_t> name '" + name + "'",
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

            std::unordered_set<uint16_t> active;

            auto absorb_stage = [&](std::string const& s, std::string const& attr_name)
            {
                if (s.empty())
                {
                    return;
                }
                auto idx = stage_index_of(s);
                if (idx == stage_data_.size())
                {
                    result.diagnostics.push_back(
                        make_build_diagnostic(BuildDiagnostic::Kind::UnknownStageOnPin,
                                  "stage '" + s + "' is not declared on the graph",
                                  node.id, attr_name));
                    return;
                }
                active.insert(static_cast<uint16_t>(idx));
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
                    make_build_diagnostic(BuildDiagnostic::Kind::NodeNeverScheduled,
                              "node has no active stage",
                              node.id));
            }

            block.active_stage_indices.assign(active.begin(), active.end());
            std::sort(block.active_stage_indices.begin(), block.active_stage_indices.end());
        }

        // ---- Per-stage topo sort (Kahn) ----
        // Use ordered containers so the resulting tick order is
        // deterministic across runs and platforms.
        for (uint16_t s = 0; s < stage_data_.size(); ++s)
        {
            std::set<piper::NodeId> in_subgraph;
            for (auto const& [id, block] : blocks_)
            {
                auto const& v = block.active_stage_indices;
                if (std::binary_search(v.begin(), v.end(), s))
                {
                    in_subgraph.insert(id);
                }
            }

            std::map<piper::NodeId, std::vector<piper::NodeId>> succ;
            std::map<piper::NodeId, std::size_t>                in_degree;
            for (auto id : in_subgraph)
            {
                in_degree[id] = 0;
            }
            for (auto const& link : effective_links)
            {
                if (in_subgraph.count(link.from.node) == 0
                    or in_subgraph.count(link.to.node) == 0)
                {
                    continue;
                }
                succ[link.from.node].push_back(link.to.node);
                ++in_degree[link.to.node];
            }
            // Sort successor lists so peers with equal precedence are
            // dequeued in NodeId order, not in the user's link-creation
            // order.
            for (auto& [_, v] : succ)
            {
                std::sort(v.begin(), v.end());
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
                std::set<piper::NodeId> remaining(in_subgraph.begin(), in_subgraph.end());
                for (auto id : order)
                {
                    remaining.erase(id);
                }
                // Prefer a real link (one carried by graph.links()) as the
                // witness. Synthetic links bypassing label clusters carry
                // invalid_link_id and would surface a non-actionable
                // Problems-panel row.
                piper::LinkId witness = piper::invalid_link_id;
                for (auto const& link : effective_links)
                {
                    if (link.id == piper::invalid_link_id) { continue; }
                    if (remaining.count(link.from.node) != 0
                        and remaining.count(link.to.node) != 0)
                    {
                        witness = link.id;
                        break;
                    }
                }
                // If only synthetic links close the cycle, fall back to a
                // remaining node so the diagnostic can at least navigate.
                piper::NodeId witness_node = piper::invalid_node_id;
                if (witness == piper::invalid_link_id and not remaining.empty())
                {
                    witness_node = *remaining.begin();
                }
                std::string message = "cycle detected in stage '" + stage_names_[s] + "'";
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnostic::Kind::CycleDetected,
                              message,
                              witness_node, std::string{}, witness));
                has_error = true;
            }

            per_stage_order_[s] = std::move(order);
        }

        // ---- Snapshot mode profiles ----
        for (auto const& mp : graph.mode_profiles())
        {
            mode_labels_.emplace(mp.name, mp.per_node);
        }
        if (not graph.default_mode_name().empty())
        {
            set_mode(graph.default_mode_name());
        }

        ok_ = not has_error;
        result.ok = ok_;
        return result;
    }

    void Engine::set_mode(std::string_view name)
    {
        current_mode_name_.assign(name.begin(), name.end());
        current_mode_ = Mode{current_mode_name_};
        active_disabled_.clear();
        for (auto& [_, block] : blocks_)
        {
            block.label_buf.clear();
            block.current_label = Mode{};
        }

        auto it = mode_labels_.find(current_mode_name_);
        if (it == mode_labels_.end())
        {
            return;
        }
        for (auto const& [id, label] : it->second)
        {
            auto bit = blocks_.find(id);
            if (bit != blocks_.end())
            {
                bit->second.label_buf     = label;
                bit->second.current_label = Mode{bit->second.label_buf};
            }
            if (label == "disable")
            {
                active_disabled_.insert(id);
            }
        }
    }

    void Engine::tick_at(std::size_t idx)
    {
        Stage const& stage = stage_data_[idx];
        auto const&  order = per_stage_order_[idx];
        for (auto id : order)
        {
            if (active_disabled_.count(id) != 0)
            {
                continue;
            }
            auto bit = blocks_.find(id);
            if (bit == blocks_.end())
            {
                continue;
            }
            bit->second.step->compute(stage);
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
