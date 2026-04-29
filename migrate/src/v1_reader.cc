#include "piper/migrate/v1_reader.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "piper/attribute.h"
#include "piper/diagnostic.h"
#include "piper/graph.h"
#include "piper/node.h"
#include "piper/node_type.h"

namespace piper::migrate
{
    using nlohmann::json;

    // V1 stored every attribute value as a stringified QVariant. The
    // V2 in-memory model also keeps values as strings; the on-disk
    // typing happens at serialize time. So we coerce numbers and bools
    // back to their string form here -- the V2 serializer will retype
    // them based on the spec's data_type when the file is written.
    std::string v1_value_to_string(json const& v)
    {
        if (v.is_string())
        {
            return v.get<std::string>();
        }
        if (v.is_null())
        {
            return std::string{};
        }
        return v.dump();
    }

    void parse_pipeline_nodes_links(json const&         pipeline_json,
                                     NodeRegistry const& registry,
                                     v2::Pipeline&       out)
    {
        std::unordered_map<std::string, NodeId> name_to_id;

        if (auto it = pipeline_json.find("Nodes");
            it != pipeline_json.end() and it->is_object())
        {
            for (auto const& [v1_name, node_json] : it->items())
            {
                if (not node_json.is_object())
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::SchemaError;
                    d.message = "node '" + v1_name + "' is not an object";
                    out.diagnostics.push_back(d);
                    continue;
                }
                std::string const type  = node_json.value("type",  std::string{});
                std::string const stage = node_json.value("stage", std::string{});

                if (type.empty())
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::SchemaError;
                    d.message = "node '" + v1_name + "' missing required 'type' field";
                    out.diagnostics.push_back(d);
                    continue;
                }

                NodeType const* nt = registry.find(type);
                if (nt == nullptr)
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::UnknownNodeType;
                    d.message = "node '" + v1_name + "' has unknown type '" + type + "'";
                    out.diagnostics.push_back(d);
                    continue;
                }

                NodeId const id = out.graph.add_node(*nt, v1_name, stage, Point{});
                name_to_id[v1_name] = id;

                // Apply attribute values from V1's flat key/value map.
                // V1 reserved "type" and "stage" as node-level fields
                // and never wrote them as attributes -- skip them on
                // read too.
                for (auto const& [key, val] : node_json.items())
                {
                    if (key == "type" or key == "stage")
                    {
                        continue;
                    }
                    bool found = false;
                    for (auto const& spec : nt->attributes)
                    {
                        if (spec.name == key)
                        {
                            found = true;
                            break;
                        }
                    }
                    if (not found)
                    {
                        Diagnostic d;
                        d.kind      = DiagnosticKind::AttributeMissing;
                        d.message   = "node '" + v1_name + "' has attribute '"
                                      + key + "' not in registry type '" + type + "'";
                        d.node_id   = id;
                        d.attr_name = key;
                        out.diagnostics.push_back(d);
                        continue;
                    }
                    out.graph.set_attr_value(id, key, v1_value_to_string(val));
                }
            }
        }

        if (auto it = pipeline_json.find("Links");
            it != pipeline_json.end() and it->is_array())
        {
            for (auto const& link_json : *it)
            {
                if (not link_json.is_object())
                {
                    out.diagnostics.push_back({
                        DiagnosticKind::SchemaError,
                        "link entry is not an object",
                        invalid_node_id, std::string{}, invalid_link_id });
                    continue;
                }
                std::string const from_name = link_json.value("from", std::string{});
                std::string const out_attr  = link_json.value("out",  std::string{});
                std::string const to_name   = link_json.value("to",   std::string{});
                std::string const in_attr   = link_json.value("in",   std::string{});
                std::string const data_type = link_json.value("type", std::string{});

                auto const it_from = name_to_id.find(from_name);
                auto const it_to   = name_to_id.find(to_name);
                if (it_from == name_to_id.end() or it_to == name_to_id.end())
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::LinkOrphanedNode;
                    d.message = "link from '" + from_name + "." + out_attr
                              + "' to '" + to_name + "." + in_attr
                              + "' references unknown node";
                    out.diagnostics.push_back(d);
                    continue;
                }

                PinRef const from{ it_from->second, out_attr };
                PinRef const to  { it_to->second,   in_attr  };
                LinkId const id  = out.graph.add_link(from, to, data_type);
                if (id == invalid_link_id)
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::LinkOrphanedAttribute;
                    d.message = "link from '" + from_name + "." + out_attr
                              + "' to '" + to_name + "." + in_attr
                              + "' references unknown attribute";
                    out.diagnostics.push_back(d);
                }
            }
        }
    }

    v2::BundleLoadResult read_v1(std::string_view     jsonstr,
                                  NodeRegistry const& registry,
                                  Options const&)
    {
        json doc;
        try
        {
            doc = json::parse(jsonstr);
        }
        catch (json::parse_error const& e)
        {
            throw std::runtime_error(std::string("malformed V1 JSON: ") + e.what());
        }
        if (not doc.is_object())
        {
            throw std::runtime_error("V1 file: expected top-level object keyed by pipeline name");
        }

        v2::BundleLoadResult result;
        for (auto const& [pipeline_name, pipeline_json] : doc.items())
        {
            if (not pipeline_json.is_object())
            {
                Diagnostic d;
                d.kind    = DiagnosticKind::SchemaError;
                d.message = "pipeline '" + pipeline_name + "' is not an object";
                result.diagnostics.push_back(d);
                continue;
            }
            v2::Pipeline pipe;
            pipe.name = pipeline_name;
            parse_pipeline_nodes_links(pipeline_json, registry, pipe);
            result.pipelines.push_back(std::move(pipe));
        }
        return result;
    }
}
