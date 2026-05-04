#include "piper/engine/label_resolver.h"

#include <map>
#include <string>
#include <unordered_set>

#include "piper/label.h"

namespace piper::engine
{
    BuildDiagnostic make_label_diagnostic(BuildDiagnostic::Kind kind,
                                          std::string           message,
                                          piper::NodeId         id)
    {
        BuildDiagnostic d;
        d.kind      = kind;
        d.message   = std::move(message);
        d.node_id   = id;
        d.attr_name = piper::label_pin_name;
        return d;
    }

    std::vector<piper::Link>
    resolve_label_clusters(piper::Graph const&            graph,
                           std::vector<BuildDiagnostic>&  diags,
                           bool&                          has_error)
    {
        struct Cluster
        {
            std::vector<piper::Label const*> sources;
            std::vector<piper::Label const*> sinks;
        };
        std::map<std::string, Cluster>    clusters;
        std::unordered_set<piper::NodeId> label_ids;
        for (auto const& l : graph.labels())
        {
            label_ids.insert(l.id);
            if (l.name.empty()) { continue; }
            Cluster& c = clusters[l.name];
            if (l.kind == piper::LabelKind::In) { c.sources.push_back(&l); }
            else                                 { c.sinks.push_back(&l);   }
        }

        std::vector<piper::Link> effective;
        effective.reserve(graph.links().size());
        for (auto const& l : graph.links())
        {
            if (label_ids.count(l.from.node) == 0
                and label_ids.count(l.to.node) == 0)
            {
                effective.push_back(l);
            }
        }

        for (auto const& [name, c] : clusters)
        {
            if (c.sources.size() > 1)
            {
                for (piper::Label const* s : c.sources)
                {
                    diags.push_back(make_label_diagnostic(
                        BuildDiagnostic::Kind::UnresolvedInput,
                        "label cluster '" + name + "' has multiple label_in nodes",
                        s->id));
                }
                has_error = true;
                continue;
            }
            if (c.sources.empty())
            {
                if (not c.sinks.empty())
                {
                    diags.push_back(make_label_diagnostic(
                        BuildDiagnostic::Kind::UnresolvedInput,
                        "label cluster '" + name + "' has label_out(s) but no label_in",
                        c.sinks.front()->id));
                    has_error = true;
                }
                continue;
            }
            if (c.sinks.empty())
            {
                diags.push_back(make_label_diagnostic(
                    BuildDiagnostic::Kind::UnresolvedInput,
                    "label cluster '" + name + "' has label_in but no label_out",
                    c.sources.front()->id));
                has_error = true;
                continue;
            }

            piper::Label const* source     = c.sources.front();
            piper::Link const*  upstream   = nullptr;
            bool                chained_in = false;
            for (auto const& l : graph.links())
            {
                if (l.to.node != source->id) { continue; }
                if (label_ids.count(l.from.node) > 0)
                {
                    chained_in = true;
                    break;
                }
                upstream = &l;
                break;
            }
            if (chained_in)
            {
                diags.push_back(make_label_diagnostic(
                    BuildDiagnostic::Kind::UnresolvedInput,
                    "label cluster '" + name + "' is fed by another label",
                    source->id));
                has_error = true;
                continue;
            }
            if (upstream == nullptr)
            {
                diags.push_back(make_label_diagnostic(
                    BuildDiagnostic::Kind::UnresolvedInput,
                    "label_in '" + name + "' has no wired source",
                    source->id));
                has_error = true;
                continue;
            }

            for (piper::Label const* sink : c.sinks)
            {
                for (auto const& l : graph.links())
                {
                    if (l.from.node != sink->id) { continue; }
                    if (label_ids.count(l.to.node) > 0)
                    {
                        diags.push_back(make_label_diagnostic(
                            BuildDiagnostic::Kind::UnresolvedInput,
                            "label cluster '" + name + "' feeds another label",
                            sink->id));
                        has_error = true;
                        continue;
                    }
                    piper::Link synthetic{};
                    synthetic.id        = piper::invalid_link_id;
                    synthetic.from      = upstream->from;
                    synthetic.to        = l.to;
                    synthetic.data_type = upstream->data_type;
                    effective.push_back(synthetic);
                }
            }
        }

        return effective;
    }
}
