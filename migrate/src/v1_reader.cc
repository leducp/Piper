#include "piper/migrate/v1_reader.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "piper/attribute.h"
#include "piper/diagnostic.h"
#include "piper/graph.h"
#include "piper/node.h"
#include "piper/node_type.h"
#include "piper/stage.h"

namespace piper::migrate
{
    using nlohmann::json;

    // V1 stored every attribute value as a stringified QVariant. The
    // V2 in-memory model also keeps values as strings; the on-disk
    // typing happens at serialize time.
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

    // V1's mode strings were written PascalCase ("Enable", "Disable",
    // "Neutral"). V2 normalizes to lowercase: "enable" and "disable"
    // are built-ins, anything else (including "neutral") is opaque
    // engine-defined data preserved verbatim.
    std::string normalize_mode_label(std::string label)
    {
        std::transform(label.begin(), label.end(), label.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return label;
    }

    void parse_stages(json const& pipeline_json, v2::Pipeline& out)
    {
        auto it = pipeline_json.find("Stages");
        if (it == pipeline_json.end() or not it->is_array())
        {
            return;
        }
        for (auto const& s : *it)
        {
            if (not s.is_string())
            {
                Diagnostic d;
                d.kind    = Diagnostic::Kind::SchemaError;
                d.message = "stage entry is not a string";
                out.diagnostics.push_back(d);
                continue;
            }
            Stage stage;
            stage.name = s.get<std::string>();
            if (not out.graph.add_stage(stage))
            {
                Diagnostic d;
                d.kind    = Diagnostic::Kind::DuplicateStageName;
                d.message = "duplicate stage name '" + stage.name + "'";
                out.diagnostics.push_back(d);
            }
        }
    }

    void parse_nodes(json const&         pipeline_json,
                     NodeRegistry const& registry,
                     v2::Pipeline&       out,
                     std::unordered_map<std::string, NodeId>& name_to_id)
    {
        auto it = pipeline_json.find("Nodes");
        if (it == pipeline_json.end() or not it->is_object())
        {
            return;
        }
        for (auto const& [v1_name, node_json] : it->items())
        {
            if (not node_json.is_object())
            {
                Diagnostic d;
                d.kind    = Diagnostic::Kind::SchemaError;
                d.message = "node '" + v1_name + "' is not an object";
                out.diagnostics.push_back(d);
                continue;
            }
            std::string type;
            std::string stage;
            try
            {
                type  = node_json.value("type",  std::string{});
                stage = node_json.value("stage", std::string{});
            }
            catch (json::exception const& e)
            {
                Diagnostic d;
                d.kind    = Diagnostic::Kind::SchemaError;
                d.message = "node '" + v1_name + "' has malformed field: " + e.what();
                out.diagnostics.push_back(d);
                continue;
            }

            if (type.empty())
            {
                Diagnostic d;
                d.kind    = Diagnostic::Kind::SchemaError;
                d.message = "node '" + v1_name + "' missing required 'type' field";
                out.diagnostics.push_back(d);
                continue;
            }

            NodeType const* nt = registry.find(type);
            if (nt == nullptr)
            {
                Diagnostic d;
                d.kind    = Diagnostic::Kind::UnknownNodeType;
                d.message = "node '" + v1_name + "' has unknown type '" + type + "'";
                out.diagnostics.push_back(d);
                continue;
            }

            NodeId const id = out.graph.add_node(*nt, v1_name, stage, Point{});
            name_to_id[v1_name] = id;

            // V1 reserved "type" and "stage" as node-level fields.
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
                    d.kind      = Diagnostic::Kind::AttributeMissing;
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

    void parse_links(json const& pipeline_json,
                     v2::Pipeline& out,
                     std::unordered_map<std::string, NodeId> const& name_to_id)
    {
        auto it = pipeline_json.find("Links");
        if (it == pipeline_json.end() or not it->is_array())
        {
            return;
        }
        for (auto const& link_json : *it)
        {
            if (not link_json.is_object())
            {
                out.diagnostics.push_back({
                    Diagnostic::Kind::SchemaError,
                    "link entry is not an object",
                    invalid_node_id, std::string{}, invalid_link_id });
                continue;
            }
            std::string from_name;
            std::string out_attr;
            std::string to_name;
            std::string in_attr;
            std::string data_type;
            try
            {
                from_name = link_json.value("from", std::string{});
                out_attr  = link_json.value("out",  std::string{});
                to_name   = link_json.value("to",   std::string{});
                in_attr   = link_json.value("in",   std::string{});
                data_type = link_json.value("type", std::string{});
            }
            catch (json::exception const& e)
            {
                Diagnostic d;
                d.kind    = Diagnostic::Kind::SchemaError;
                d.message = std::string("link entry has malformed field: ") + e.what();
                out.diagnostics.push_back(d);
                continue;
            }

            auto const it_from = name_to_id.find(from_name);
            auto const it_to   = name_to_id.find(to_name);
            if (it_from == name_to_id.end() or it_to == name_to_id.end())
            {
                Diagnostic d;
                d.kind    = Diagnostic::Kind::LinkOrphanedNode;
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
                d.kind    = Diagnostic::Kind::LinkOrphanedAttribute;
                d.message = "link from '" + from_name + "." + out_attr
                          + "' to '" + to_name + "." + in_attr
                          + "' references unknown attribute";
                out.diagnostics.push_back(d);
            }
        }
    }

    void parse_modes(json const&                                    pipeline_json,
                     v2::Pipeline&                                  out,
                     std::unordered_map<std::string, NodeId> const& name_to_id)
    {
        auto it = pipeline_json.find("Modes");
        if (it == pipeline_json.end() or not it->is_object())
        {
            return;
        }
        for (auto const& [key, value] : it->items())
        {
            // V1 stored the default-profile name under the reserved
            // "default" key at the top of Modes.
            if (key == "default")
            {
                if (value.is_string())
                {
                    out.graph.set_default_mode_name(value.get<std::string>());
                }
                continue;
            }
            if (not value.is_object())
            {
                Diagnostic d;
                d.kind    = Diagnostic::Kind::SchemaError;
                d.message = "mode profile '" + key + "' is not an object";
                out.diagnostics.push_back(d);
                continue;
            }

            try
            {
                ModeProfile profile;
                profile.name = key;

                auto config_it = value.find("configuration");
                if (config_it != value.end() and config_it->is_object())
                {
                    for (auto const& [node_name, label_json] : config_it->items())
                    {
                        if (not label_json.is_string())
                        {
                            continue;
                        }
                        auto name_it = name_to_id.find(node_name);
                        if (name_it == name_to_id.end())
                        {
                            Diagnostic d;
                            d.kind    = Diagnostic::Kind::OrphanModeReference;
                            d.message = "mode profile '" + key + "' references unknown node '"
                                      + node_name + "'";
                            out.diagnostics.push_back(d);
                            continue;
                        }
                        profile.per_node[name_it->second] =
                            normalize_mode_label(label_json.get<std::string>());
                    }
                }

                if (not out.graph.add_mode_profile(profile))
                {
                    Diagnostic d;
                    d.kind    = Diagnostic::Kind::DuplicateProfileName;
                    d.message = "duplicate mode profile name '" + profile.name + "'";
                    out.diagnostics.push_back(d);
                }
            }
            catch (json::exception const& e)
            {
                Diagnostic d;
                d.kind    = Diagnostic::Kind::SchemaError;
                d.message = "mode profile '" + key + "' has malformed field: " + e.what();
                out.diagnostics.push_back(d);
            }
        }
    }

    void parse_pipeline(json const&         pipeline_json,
                        NodeRegistry const& registry,
                        v2::Pipeline&       out)
    {
        // Order matters: stages first (nodes may reference them by
        // name), nodes before links (links reference node ids), modes
        // last (per_node references node ids assigned during nodes).
        std::unordered_map<std::string, NodeId> name_to_id;
        parse_stages(pipeline_json, out);
        parse_nodes(pipeline_json, registry, out, name_to_id);
        parse_links(pipeline_json, out, name_to_id);
        parse_modes(pipeline_json, out, name_to_id);
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
        // V2/V3 files carry an integer top-level "version"; V1 keys
        // every top-level entry by pipeline name.
        auto const version_it = doc.find("version");
        if (version_it != doc.end()
            and version_it->is_number_integer()
            and version_it->get<int>() >= 2)
        {
            throw std::runtime_error(
                "input is already a v2/v3 piper file (version "
                + std::to_string(version_it->get<int>())
                + "); refusing to migrate it");
        }

        v2::BundleLoadResult result;
        for (auto const& [pipeline_name, pipeline_json] : doc.items())
        {
            if (pipeline_name == "version")
            {
                continue;
            }
            if (not pipeline_json.is_object())
            {
                Diagnostic d;
                d.kind    = Diagnostic::Kind::SchemaError;
                d.message = "pipeline '" + pipeline_name + "' is not an object";
                result.diagnostics.push_back(d);
                continue;
            }
            v2::Pipeline pipe;
            pipe.name = pipeline_name;
            parse_pipeline(pipeline_json, registry, pipe);
            result.pipelines.push_back(std::move(pipe));
        }
        return result;
    }
}
