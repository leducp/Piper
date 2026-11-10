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
        external_inputs_.clear();
        external_outputs_.clear();
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
        current_mode_name_ = graph.default_mode_name();
        current_mode_ = Mode{current_mode_name_};
        for (auto const& mp : graph.mode_profiles())
        {
            mode_labels_.emplace(mp.name, mp.per_node);
        }

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

            auto errors = node.member_configuration_errors();
            if (node.members_per_label)
            {
                for (auto const& profile : graph.mode_profiles())
                {
                    auto entry = profile.per_node.find(node.id);
                    std::string label;
                    if (entry != profile.per_node.end())
                    {
                        label = entry->second;
                    }
                    if (node.label_members.count(label) == 0)
                    {
                        errors.push_back("profile '" + profile.name +
                                         "': missing member set for label '" + label + "'");
                    }
                }
            }
            for (auto const& error : errors)
            {
                result.diagnostics.push_back(make_build_diagnostic(
                    BuildDiagnostic::Kind::InvalidMemberConfiguration, error, node.id));
                has_error = true;
            }
            if (not errors.empty())
            {
                continue;
            }
            if (auto profile = mode_labels_.find(current_mode_name_); profile != mode_labels_.end())
            {
                if (auto label = profile->second.find(node.id); label != profile->second.end())
                {
                    block.label_buf = label->second;
                }
            }
            block.current_label = Mode{};
            if (not block.label_buf.empty())
            {
                block.current_label = Mode{block.label_buf};
            }
            if (block.label_buf == "disable")
            {
                active_disabled_.insert(node.id);
            }
            block.members = node.members_for_label(block.label_buf);
            block.label_members = node.label_members;

            block.current_mode = &current_mode_;
            block.step->init(block);
            try
            {
                block.step->declare_io();
                block.step->on_label_changed();
            }
            catch (std::exception const& e)
            {
                result.diagnostics.push_back(make_build_diagnostic(
                    BuildDiagnostic::Kind::StepDeclareIoFailed,
                    std::string{"declare_io/on_label_changed threw for label '"} + block.label_buf +
                        "': " + e.what(),
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

        // Index external I/O storage by the names saved in the attributes.
        for (auto const& node : graph.nodes())
        {
            auto block_it = blocks_.find(node.id);
            if (block_it == blocks_.end())
            {
                continue;
            }
            auto& block = block_it->second;

            auto conflict = [&](std::string message, std::string const& attr_name)
            {
                result.diagnostics.push_back(
                    make_build_diagnostic(BuildDiagnostic::Kind::ExternalIoConflict,
                                          std::move(message), node.id, attr_name));
                has_error = true;
            };

            // Each step declaration must match a graph attribute with the same role.
            auto check_declared = [&](auto const& declared,
                                      piper::AttributeSpec::Role role,
                                      char const* what)
            {
                for (auto const& [attr_name, slot] : declared)
                {
                    piper::Attribute const* a = node.find_attr(attr_name);
                    if (a == nullptr or a->role != role)
                    {
                        conflict(std::string{"step declares "} + what + " '" + attr_name
                                     + "' but the node has no " + what + " attribute of that name",
                                 attr_name);
                    }
                }
            };
            check_declared(block.external_inputs,
                           piper::AttributeSpec::Role::ExternalInput, "external input");
            check_declared(block.external_outputs,
                           piper::AttributeSpec::Role::ExternalOutput, "external output");

            for (auto const& attr : node.attrs)
            {
                if (not piper::is_external(attr.role))
                {
                    continue;
                }
                bool const is_input = attr.role == piper::AttributeSpec::Role::ExternalInput;
                char const* what         = "external output";
                char const* declare_call = " (declare_external_output)";
                auto const* declared     = &block.external_outputs;
                ExternalTable* table     = &external_outputs_;
                if (is_input)
                {
                    what         = "external input";
                    declare_call = " (declare_external_input)";
                    declared     = &block.external_inputs;
                    table        = &external_inputs_;
                }

                if (block.input_slots.count(attr.name) != 0
                    or block.output_slots.count(attr.name) != 0)
                {
                    conflict(std::string{what} + " '" + attr.name
                                 + "' is declared by the step as a linkable pin",
                             attr.name);
                    continue;
                }

                auto slot_it = declared->find(attr.name);
                if (slot_it == declared->end())
                {
                    conflict(std::string{what} + " '" + attr.name + "' is not declared by the step"
                                 + declare_call,
                             attr.name);
                    continue;
                }

                if (attr.data_type != slot_it->second.data_type)
                {
                    conflict(std::string{what} + " '" + attr.name + "' is declared as '"
                                 + attr.data_type + "' in the graph, but the step exposes '"
                                 + slot_it->second.data_type + "'",
                             attr.name);
                    continue;
                }

                if (attr.value.empty())
                {
                    // Unnamed external I/O is valid: it is just not exported.
                    continue;
                }

                if (not table->emplace(attr.value, &slot_it->second).second)
                {
                    conflict("duplicate " + std::string{what} + " name '" + attr.value + "'",
                             attr.name);
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
                    // An undeclared pin stage silently zeroes pin_active for
                    // every stage, dropping the edge from all ordering graphs
                    // and disarming cycle detection -- fail the build instead.
                    has_error = true;
                    return;
                }
                active.insert(static_cast<uint16_t>(idx));
            };

            absorb_stage(node.stage, std::string{});

            for (auto const& attr : node.attrs)
            {
                if (piper::is_pin(attr.role))
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

            // A scheduled step reading a declared, non-optional input
            // that no link wired throws std::out_of_range on first read
            // -- fail the build so no host (studio or embedder) ticks
            // into a crash.
            if (not active.empty())
            {
                for (auto const& [in_name, slot] : block.input_slots)
                {
                    if (slot.optional or block.inputs.count(in_name) != 0)
                    {
                        continue;
                    }
                    result.diagnostics.push_back(
                        make_build_diagnostic(BuildDiagnostic::Kind::UnresolvedInput,
                                  "required input '" + in_name + "' has no wired source",
                                  node.id, in_name));
                    has_error = true;
                }
            }
        }

        // ---- Per-stage topo sort (Kahn) ----
        // A link orders a stage only when BOTH of its pins are active
        // there -- a pin with explicit stages is active on those, else it
        // inherits its node's home stage. Filtering by node membership
        // alone would drag e.g. a write-staged back-link between two
        // read+write nodes into the read stage and report a false cycle
        // (see tests/engine/cross_stage_link-t.cc).
        std::map<piper::NodeId, std::map<std::string, std::vector<uint16_t>>> pin_stages;
        for (auto const& node : graph.nodes())
        {
            auto const home = stage_index_of(node.stage);
            for (auto const& attr : node.attrs)
            {
                if (not piper::is_pin(attr.role))
                {
                    continue;
                }
                std::vector<uint16_t> active_on;
                if (attr.stages.empty())
                {
                    if (home < stage_data_.size())
                    {
                        active_on.push_back(static_cast<uint16_t>(home));
                    }
                }
                else
                {
                    for (auto const& sname : attr.stages)
                    {
                        auto const idx = stage_index_of(sname);
                        if (idx < stage_data_.size())
                        {
                            active_on.push_back(static_cast<uint16_t>(idx));
                        }
                    }
                }
                std::sort(active_on.begin(), active_on.end());
                pin_stages[node.id][attr.name] = std::move(active_on);
            }
        }
        auto pin_active = [&pin_stages](piper::NodeId id, std::string const& attr, uint16_t s)
        {
            auto nit = pin_stages.find(id);
            if (nit == pin_stages.end())
            {
                return false;
            }
            auto ait = nit->second.find(attr);
            if (ait == nit->second.end())
            {
                return false;
            }
            return std::binary_search(ait->second.begin(), ait->second.end(), s);
        };

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
                if (not pin_active(link.from.node, link.from.attr, s)
                    or not pin_active(link.to.node, link.to.attr, s))
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

        ok_ = not has_error;
        result.ok = ok_;
        return result;
    }

    void Engine::set_mode(std::string_view name)
    {
        current_mode_name_.assign(name.begin(), name.end());
        current_mode_ = Mode{current_mode_name_};
        active_disabled_.clear();
        auto profile = mode_labels_.find(current_mode_name_);
        std::vector<IoBlock*> changed;
        for (auto& [id, block] : blocks_)
        {
            std::string label;
            if (profile != mode_labels_.end())
            {
                if (auto it = profile->second.find(id); it != profile->second.end())
                {
                    label = it->second;
                }
            }
            if (label == "disable")
            {
                active_disabled_.insert(id);
            }
            if (label == block.label_buf)
            {
                continue;
            }
            block.label_buf = std::move(label);
            block.current_label = Mode{};
            if (not block.label_buf.empty())
            {
                block.current_label = Mode{block.label_buf};
            }
            if (not block.label_members.empty())
            {
                block.members = block.label_members.at(block.label_buf);
            }
            changed.push_back(&block);
        }
        for (auto* block : changed)
        {
            block->step->init(*block);
            try
            {
                block->step->on_label_changed();
            }
            catch (std::exception const& e)
            {
                ok_ = false;
                throw std::runtime_error("node " + std::to_string(block->node_id) + " label '" +
                                         block->label_buf +
                                         "': on_label_changed failed: " + e.what());
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
            // Rebind before compute: a step instance may be shared by
            // several engines (hardware singletons); input()/output()
            // must resolve against THIS engine's wiring.
            bit->second.step->init(bit->second);
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

    ExternalSlot const* Engine::find_external(ExternalTable const& table,
                                              std::string_view name)
    {
        auto it = table.find(name);
        if (it == table.end())
        {
            return nullptr;
        }
        return it->second;
    }
}
