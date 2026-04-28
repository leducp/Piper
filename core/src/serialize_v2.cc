#include "piper/serialize_v2.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "piper/attribute.h"
#include "piper/color.h"
#include "piper/link.h"
#include "piper/mode_profile.h"
#include "piper/node.h"
#include "piper/node_type.h"
#include "piper/rgba_io.h"
#include "piper/stage.h"

#include "diagnostic_helpers.h"

namespace piper::v2
{
    using nlohmann::json;

    char const* role_to_str(AttributeSpec::Role r)
    {
        switch (r)
        {
            case AttributeSpec::Role::Input:
            {
                return "input";
            }
            case AttributeSpec::Role::Output:
            {
                return "output";
            }
            case AttributeSpec::Role::Member:
            {
                return "member";
            }
        }
        std::abort();   // exhaustive switch over scoped enum
    }

    bool role_from_str(std::string_view s, AttributeSpec::Role& out)
    {
        if (s == "input")
        {
            out = AttributeSpec::Role::Input;
            return true;
        }
        if (s == "output")
        {
            out = AttributeSpec::Role::Output;
            return true;
        }
        if (s == "member")
        {
            out = AttributeSpec::Role::Member;
            return true;
        }
        return false;
    }

    std::string serialize(Graph const& g)
    {
        json doc;
        doc["version"] = format_version;
        doc["nodes"]   = json::array();
        doc["links"]   = json::array();
        doc["stages"]  = json::array();
        doc["modes"]   = json::array();

        for (auto const& n : g.nodes())
        {
            json node_json;
            node_json["id"]    = n.id;
            node_json["type"]  = n.type;
            node_json["name"]  = n.name;
            node_json["stage"] = n.stage;
            node_json["pos"]   = json::array({ n.pos.x, n.pos.y });

            json attrs_json = json::array();
            for (auto const& a : n.attrs)
            {
                json attr_json;
                attr_json["name"]      = a.name;
                attr_json["data_type"] = a.data_type;
                attr_json["role"]      = role_to_str(a.role);
                if (not a.value.empty())
                {
                    attr_json["value"] = a.value;
                }
                if (not a.stages.empty())
                {
                    attr_json["stages"] = a.stages;
                }
                attrs_json.push_back(attr_json);
            }
            node_json["attrs"] = attrs_json;
            doc["nodes"].push_back(node_json);
        }

        for (auto const& l : g.links())
        {
            json link_json;
            link_json["id"]        = l.id;
            link_json["from"]      = json::object({ {"node", l.from.node}, {"attr", l.from.attr} });
            link_json["to"]        = json::object({ {"node", l.to.node},   {"attr", l.to.attr}   });
            link_json["data_type"] = l.data_type;
            doc["links"].push_back(link_json);
        }

        for (auto const& s : g.stages())
        {
            json stage_json;
            stage_json["name"]  = s.name;
            stage_json["color"] = format_rgba(s.color);
            doc["stages"].push_back(stage_json);
        }

        for (auto const& m : g.mode_profiles())
        {
            json mode_json;
            mode_json["name"]       = m.name;
            mode_json["is_default"] = m.is_default;

            // Sort per_node by NodeId so output is deterministic across runs.
            std::vector<std::pair<NodeId, std::string>> entries(
                m.per_node.begin(), m.per_node.end());
            std::sort(entries.begin(), entries.end(),
                      [](auto const& a, auto const& b) { return a.first < b.first; });

            json per_node_json = json::array();
            for (auto const& [node_id, label] : entries)
            {
                per_node_json.push_back(json::object({ {"node", node_id}, {"label", label} }));
            }
            mode_json["per_node"] = per_node_json;
            doc["modes"].push_back(mode_json);
        }

        return doc.dump(2);
    }

    bool parse_node(json const& node_json,
                    Node& out,
                    std::vector<Diagnostic>& diags)
    {
        if (not node_json.contains("id") or not node_json.contains("type"))
        {
            diags.push_back(schema_error("node missing required 'id' or 'type' field"));
            return false;
        }
        try
        {
            out.id    = node_json.at("id").get<NodeId>();
            out.type  = node_json.at("type").get<std::string>();
            out.name  = node_json.value("name",  std::string{});
            out.stage = node_json.value("stage", std::string{});

            if (auto pos_it = node_json.find("pos"); pos_it != node_json.end())
            {
                if (pos_it->is_array() and pos_it->size() == 2)
                {
                    out.pos.x = pos_it->at(0).get<float>();
                    out.pos.y = pos_it->at(1).get<float>();
                }
                else
                {
                    diags.push_back(schema_error(
                        "node " + std::to_string(out.id)
                        + " has malformed 'pos' (expected [x, y])"));
                }
            }

            if (auto attrs_it = node_json.find("attrs"); attrs_it != node_json.end() and attrs_it->is_array())
            {
                for (auto const& attr_json : *attrs_it)
                {
                    if (not attr_json.contains("name") or not attr_json.contains("data_type") or not attr_json.contains("role"))
                    {
                        diags.push_back(schema_error(
                            "attribute on node " + std::to_string(out.id)
                            + " missing required 'name', 'data_type', or 'role'"));
                        continue;
                    }
                    Attribute a;
                    a.name      = attr_json.at("name").get<std::string>();
                    a.data_type = attr_json.at("data_type").get<std::string>();
                    std::string role_str = attr_json.at("role").get<std::string>();
                    if (not role_from_str(role_str, a.role))
                    {
                        diags.push_back(schema_error(
                            "attribute '" + a.name + "' on node " + std::to_string(out.id)
                            + " has unknown role '" + role_str + "'"));
                        continue;
                    }
                    a.value  = attr_json.value("value",  std::string{});
                    a.stages = attr_json.value("stages", std::vector<std::string>{});
                    out.attrs.push_back(a);
                }
            }
        }
        catch (json::exception const& e)
        {
            diags.push_back(schema_error(std::string("node parse error: ") + e.what()));
            return false;
        }
        return true;
    }

    bool parse_link(json const& link_json,
                    Link& out,
                    std::vector<Diagnostic>& diags)
    {
        if (not link_json.contains("id") or not link_json.contains("from") or not link_json.contains("to"))
        {
            diags.push_back(schema_error("link missing required 'id', 'from', or 'to'"));
            return false;
        }
        try
        {
            out.id = link_json.at("id").get<LinkId>();
            out.from.node = link_json.at("from").at("node").get<NodeId>();
            out.from.attr = link_json.at("from").at("attr").get<std::string>();
            out.to.node   = link_json.at("to").at("node").get<NodeId>();
            out.to.attr   = link_json.at("to").at("attr").get<std::string>();
            out.data_type = link_json.value("data_type", std::string{});
        }
        catch (json::exception const& e)
        {
            diags.push_back(schema_error(std::string("link parse error: ") + e.what()));
            return false;
        }
        return true;
    }

    // Walks both directions: saved attrs missing from spec -> AttributeMissing,
    // spec attrs missing from saved -> AttributeAdded, type drift -> AttributeDrift.
    void check_attribute_drift(Node const& node,
                               NodeType const& spec,
                               std::vector<Diagnostic>& diags)
    {
        for (auto const& a : node.attrs)
        {
            AttributeSpec const* s = nullptr;
            for (auto const& candidate : spec.attributes)
            {
                if (candidate.name == a.name)
                {
                    s = &candidate;
                    break;
                }
            }
            if (s == nullptr)
            {
                Diagnostic d;
                d.kind      = DiagnosticKind::AttributeMissing;
                d.message   = "node " + std::to_string(node.id) + " has saved attribute '"
                              + a.name + "' not present in registry type '" + node.type + "'";
                d.node_id   = node.id;
                d.attr_name = a.name;
                diags.push_back(d);
                continue;
            }
            if (s->data_type != a.data_type)
            {
                Diagnostic d;
                d.kind      = DiagnosticKind::AttributeDrift;
                d.message   = "node " + std::to_string(node.id) + " attribute '" + a.name
                              + "' saved as '" + a.data_type + "' but registry says '"
                              + s->data_type + "'";
                d.node_id   = node.id;
                d.attr_name = a.name;
                diags.push_back(d);
            }
        }

        for (auto const& s : spec.attributes)
        {
            bool found = false;
            for (auto const& a : node.attrs)
            {
                if (a.name == s.name)
                {
                    found = true;
                    break;
                }
            }
            if (not found)
            {
                Diagnostic d;
                d.kind      = DiagnosticKind::AttributeAdded;
                d.message   = "registry type '" + node.type + "' has attribute '"
                              + s.name + "' not present in saved node "
                              + std::to_string(node.id);
                d.node_id   = node.id;
                d.attr_name = s.name;
                diags.push_back(d);
            }
        }
    }

    void check_stage_references(Graph const& g, std::vector<Diagnostic>& diags)
    {
        auto known = [&](std::string const& name)
        {
            for (auto const& s : g.stages())
            {
                if (s.name == name)
                {
                    return true;
                }
            }
            return false;
        };

        for (auto const& n : g.nodes())
        {
            if (not n.stage.empty() and not known(n.stage))
            {
                Diagnostic d;
                d.kind    = DiagnosticKind::UnknownStageReference;
                d.message = "node " + std::to_string(n.id) + " references unknown stage '"
                            + n.stage + "'";
                d.node_id = n.id;
                diags.push_back(d);
            }
            for (auto const& a : n.attrs)
            {
                for (auto const& s : a.stages)
                {
                    if (not known(s))
                    {
                        Diagnostic d;
                        d.kind      = DiagnosticKind::UnknownStageReference;
                        d.message   = "node " + std::to_string(n.id) + " attribute '"
                                      + a.name + "' references unknown stage '" + s + "'";
                        d.node_id   = n.id;
                        d.attr_name = a.name;
                        diags.push_back(d);
                    }
                }
            }
        }
    }

    LoadResult deserialize(std::string_view jsonstr, NodeRegistry const& registry)
    {
        json doc;
        try
        {
            doc = json::parse(jsonstr);
        }
        catch (json::parse_error const& e)
        {
            throw std::runtime_error(std::string("malformed JSON: ") + e.what());
        }

        int version = doc.value("version", 0);
        if (version != format_version)
        {
            throw std::runtime_error(
                "unsupported V2 format version " + std::to_string(version)
                + " (expected " + std::to_string(format_version) + ")");
        }

        LoadResult result;
        NodeId max_node_id = 0;
        LinkId max_link_id = 0;

        if (auto it = doc.find("stages"); it != doc.end() and it->is_array())
        {
            for (auto const& stage_json : *it)
            {
                if (not stage_json.contains("name"))
                {
                    result.diagnostics.push_back(schema_error("stage missing 'name'"));
                    continue;
                }
                Stage s;
                s.name = stage_json.at("name").get<std::string>();
                if (auto color_it = stage_json.find("color"); color_it != stage_json.end() and color_it->is_string())
                {
                    auto parsed = parse_rgba(color_it->get<std::string>());
                    if (parsed.has_value())
                    {
                        s.color = *parsed;
                    }
                    else
                    {
                        // Stage's default color (opaque white) stays in effect.
                        result.diagnostics.push_back(schema_error(
                            "stage '" + s.name + "' has malformed color"));
                    }
                }
                if (not result.graph.add_stage(s))
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::DuplicateStageName;
                    d.message = "duplicate stage name '" + s.name + "'";
                    result.diagnostics.push_back(d);
                }
            }
        }

        if (auto it = doc.find("nodes"); it != doc.end() and it->is_array())
        {
            for (auto const& node_json : *it)
            {
                Node node;
                if (not parse_node(node_json, node, result.diagnostics))
                {
                    continue;
                }

                NodeType const* spec = registry.find(node.type);
                if (spec == nullptr)
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::UnknownNodeType;
                    d.message = "node " + std::to_string(node.id) + " has unknown type '"
                                + node.type + "'";
                    d.node_id = node.id;
                    result.diagnostics.push_back(d);
                }
                else
                {
                    check_attribute_drift(node, *spec, result.diagnostics);
                }

                if (node.id > max_node_id)
                {
                    max_node_id = node.id;
                }

                if (not result.graph.insert_node(node))
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::DuplicateNodeId;
                    d.message = "duplicate node id " + std::to_string(node.id);
                    d.node_id = node.id;
                    result.diagnostics.push_back(d);
                }
            }
        }

        if (auto it = doc.find("links"); it != doc.end() and it->is_array())
        {
            for (auto const& link_json : *it)
            {
                Link link;
                if (not parse_link(link_json, link, result.diagnostics))
                {
                    continue;
                }

                if (link.id > max_link_id)
                {
                    max_link_id = link.id;
                }

                Node const* from_node = result.graph.find_node(link.from.node);
                Node const* to_node   = result.graph.find_node(link.to.node);

                if (from_node == nullptr or to_node == nullptr)
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::LinkOrphanedNode;
                    d.message = "link " + std::to_string(link.id) + " references unknown node";
                    d.link_id = link.id;
                    result.diagnostics.push_back(d);
                    continue;
                }

                Attribute const* from_attr = from_node->find_attr(link.from.attr);
                Attribute const* to_attr   = to_node->find_attr(link.to.attr);
                if (from_attr == nullptr or to_attr == nullptr)
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::LinkOrphanedAttribute;
                    d.message = "link " + std::to_string(link.id) + " references unknown attribute";
                    d.link_id = link.id;
                    result.diagnostics.push_back(d);
                    continue;
                }

                bool type_mismatch = false;
                if (not link.data_type.empty()
                    and (link.data_type != from_attr->data_type
                         or link.data_type != to_attr->data_type))
                {
                    type_mismatch = true;
                }
                else if (from_attr->data_type != to_attr->data_type)
                {
                    type_mismatch = true;
                }

                if (type_mismatch)
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::LinkTypeMismatch;
                    d.link_id = link.id;
                    if (not link.data_type.empty())
                    {
                        d.message = "link " + std::to_string(link.id) + " declared data_type '"
                                    + link.data_type + "' but endpoints are '"
                                    + from_attr->data_type + "' and '" + to_attr->data_type + "'";
                    }
                    else
                    {
                        d.message = "link " + std::to_string(link.id)
                                    + " endpoints have mismatched types: '"
                                    + from_attr->data_type + "' vs '" + to_attr->data_type + "'";
                    }
                    result.diagnostics.push_back(d);
                }

                // LinkTypeMismatch only: insertion proceeds -- the editor
                // surfaces the diagnostic and lets the user re-route.
                // (LinkOrphanedNode and LinkOrphanedAttribute above used
                // `continue` and dropped the link entirely.) Engine
                // consumers must check LoadResult::diagnostics before
                // trusting any link.
                if (not result.graph.insert_link(link))
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::DuplicateLinkId;
                    d.message = "duplicate link id " + std::to_string(link.id);
                    d.link_id = link.id;
                    result.diagnostics.push_back(d);
                }
            }
        }

        if (auto it = doc.find("modes"); it != doc.end() and it->is_array())
        {
            for (auto const& mode_json : *it)
            {
                if (not mode_json.contains("name"))
                {
                    result.diagnostics.push_back(schema_error("mode profile missing 'name'"));
                    continue;
                }
                ModeProfile m;
                m.name       = mode_json.at("name").get<std::string>();
                m.is_default = mode_json.value("is_default", false);

                if (auto pn_it = mode_json.find("per_node"); pn_it != mode_json.end() and pn_it->is_array())
                {
                    for (auto const& entry : *pn_it)
                    {
                        if (not entry.contains("node") or not entry.contains("label"))
                        {
                            result.diagnostics.push_back(schema_error(
                                "mode profile '" + m.name + "' per_node entry missing 'node' or 'label'"));
                            continue;
                        }
                        NodeId nid       = entry.at("node").get<NodeId>();
                        std::string label = entry.at("label").get<std::string>();

                        if (result.graph.find_node(nid) == nullptr)
                        {
                            Diagnostic d;
                            d.kind    = DiagnosticKind::OrphanModeReference;
                            d.message = "mode profile '" + m.name + "' references unknown node "
                                        + std::to_string(nid);
                            d.node_id = nid;
                            result.diagnostics.push_back(d);
                        }
                        m.per_node[nid] = label;
                    }
                }

                if (not result.graph.add_mode_profile(m))
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::DuplicateProfileName;
                    d.message = "duplicate mode profile name '" + m.name + "'";
                    result.diagnostics.push_back(d);
                }
            }
        }

        check_stage_references(result.graph, result.diagnostics);

        result.graph.reserve_ids_above(max_node_id, max_link_id);
        return result;
    }

    // ---- Registry (engine's node-type catalog) ----

    std::string serialize_registry(NodeRegistry const& reg)
    {
        json doc;
        doc["version"] = format_version;
        doc["types"]   = json::array();

        // Sort by type name so output is stable across runs.
        auto types = reg.all();
        std::sort(types.begin(), types.end(),
                  [](NodeType const* a, NodeType const* b) { return a->type < b->type; });

        for (auto const* nt : types)
        {
            json type_json;
            type_json["type"] = nt->type;
            if (not nt->library.empty())
            {
                type_json["library"] = nt->library;
            }
            if (not nt->category.empty())
            {
                type_json["category"] = nt->category;
            }
            if (not nt->help.empty())
            {
                type_json["help"] = nt->help;
            }

            json attrs_json = json::array();
            for (auto const& spec : nt->attributes)
            {
                json attr_json;
                attr_json["name"]      = spec.name;
                attr_json["data_type"] = spec.data_type;
                attr_json["role"]      = role_to_str(spec.role);
                if (not spec.default_value.empty())
                {
                    attr_json["default_value"] = spec.default_value;
                }
                attrs_json.push_back(attr_json);
            }
            type_json["attributes"] = attrs_json;
            doc["types"].push_back(type_json);
        }

        return doc.dump(2);
    }

    bool parse_attribute_spec(json const& attr_json,
                              std::string const& type_name,
                              AttributeSpec& out,
                              std::vector<Diagnostic>& diags)
    {
        if (not attr_json.contains("name") or not attr_json.contains("data_type") or not attr_json.contains("role"))
        {
            diags.push_back(schema_error(
                "type '" + type_name + "' attribute missing required field"));
            return false;
        }
        try
        {
            out.name      = attr_json.at("name").get<std::string>();
            out.data_type = attr_json.at("data_type").get<std::string>();
            std::string role_str = attr_json.at("role").get<std::string>();
            if (not role_from_str(role_str, out.role))
            {
                diags.push_back(schema_error(
                    "type '" + type_name + "' attribute '" + out.name
                    + "' has unknown role '" + role_str + "'"));
                return false;
            }
            out.default_value = attr_json.value("default_value", std::string{});
        }
        catch (json::exception const& e)
        {
            diags.push_back(schema_error(
                std::string("attribute parse error: ") + e.what()));
            return false;
        }
        return true;
    }

    RegistryLoadResult deserialize_registry(std::string_view jsonstr)
    {
        json doc;
        try
        {
            doc = json::parse(jsonstr);
        }
        catch (json::parse_error const& e)
        {
            throw std::runtime_error(std::string("malformed JSON: ") + e.what());
        }

        int version = doc.value("version", 0);
        if (version != format_version)
        {
            throw std::runtime_error(
                "unsupported V2 format version " + std::to_string(version)
                + " (expected " + std::to_string(format_version) + ")");
        }

        RegistryLoadResult result;

        if (auto it = doc.find("types"); it != doc.end() and it->is_array())
        {
            for (auto const& type_json : *it)
            {
                if (not type_json.contains("type"))
                {
                    result.diagnostics.push_back(schema_error("type missing 'type' field"));
                    continue;
                }

                NodeType nt;
                try
                {
                    nt.type     = type_json.at("type").get<std::string>();
                    nt.library  = type_json.value("library",  std::string{});
                    nt.category = type_json.value("category", std::string{});
                    nt.help     = type_json.value("help",     std::string{});
                }
                catch (json::exception const& e)
                {
                    result.diagnostics.push_back(schema_error(
                        std::string("type parse error: ") + e.what()));
                    continue;
                }

                if (auto attrs_it = type_json.find("attributes"); attrs_it != type_json.end() and attrs_it->is_array())
                {
                    for (auto const& attr_json : *attrs_it)
                    {
                        AttributeSpec spec;
                        if (parse_attribute_spec(attr_json, nt.type, spec, result.diagnostics))
                        {
                            nt.attributes.push_back(spec);
                        }
                    }
                }

                if (not result.registry.add(nt))
                {
                    Diagnostic d;
                    d.kind    = DiagnosticKind::DuplicateTypeName;
                    d.message = "duplicate node type '" + nt.type + "'";
                    result.diagnostics.push_back(d);
                }
            }
        }

        return result;
    }
}
