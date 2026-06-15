#include <algorithm>
#include <cstdlib>
#include <stdexcept>
#include <utility>

#include <nlohmann/json.hpp>

#include "piper/serialize_v2.h"

#include "diagnostic_helpers.h"
#include "piper/attribute.h"
#include "piper/color.h"
#include "piper/link.h"
#include "piper/mode_profile.h"
#include "piper/rgba_io.h"
#include "piper/stage.h"

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

    bool is_numeric_data_type(std::string_view dt)
    {
        return dt == "float"  or dt == "double"
            or dt == "int32_t"    or dt == "uint"
            or dt == "int32"  or dt == "int64"
            or dt == "uint32" or dt == "uint64"
            or dt == "long"   or dt == "ulong"
            or dt == "size";
    }

    // Encode an Attribute's stringly-typed in-memory value into the JSON
    // shape dictated by data_type. Numerics become JSON numbers, "bool"
    // becomes a JSON boolean; everything else (and any parse failure)
    // falls back to a verbatim JSON string so user content is never lost.
    json encode_attr_value(std::string const& value, std::string const& data_type)
    {
        if (is_numeric_data_type(data_type))
        {
            try
            {
                json parsed = json::parse(value);
                if (parsed.is_number())
                {
                    return parsed;
                }
            }
            catch (json::parse_error const&)
            {
            }
            return value;
        }
        if (data_type == "bool")
        {
            if (value == "true")
            {
                return true;
            }
            if (value == "false")
            {
                return false;
            }
            return value;
        }
        return value;
    }

    // Inverse of encode_attr_value. Numbers, bools, arrays, and objects
    // are dump()'d to canonical JSON text; strings come back verbatim.
    std::string decode_attr_value(json const& v)
    {
        if (v.is_null())
        {
            return std::string{};
        }
        if (v.is_string())
        {
            return v.get<std::string>();
        }
        return v.dump();
    }

    json write_pipeline_body(Graph const& g, std::string const& name)
    {
        json doc;
        if (not name.empty())
        {
            doc["name"] = name;
        }

        if (not g.meta().empty())
        {
            json meta_json = json::object();
            for (auto const& [k, v] : g.meta())
            {
                meta_json[k] = v;
            }
            doc["meta"] = meta_json;
        }

        if (not g.default_mode_name().empty())
        {
            doc["default_mode"] = g.default_mode_name();
        }

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
            if (not n.note.empty())
            {
                node_json["note"] = n.note;
            }
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
                    attr_json["value"] = encode_attr_value(a.value, a.data_type);
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
            mode_json["name"] = m.name;

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

        if (not g.annotations().empty())
        {
            doc["annotations"] = json::array();
            for (auto const& a : g.annotations())
            {
                json an;
                an["id"]    = a.id;
                an["pos"]   = json::array({ a.pos.x,  a.pos.y  });
                an["size"]  = json::array({ a.size.x, a.size.y });
                an["color"] = format_rgba(a.color);
                if (not a.text.empty())
                {
                    an["text"] = a.text;
                }
                doc["annotations"].push_back(an);
            }
        }

        if (not g.labels().empty())
        {
            doc["labels"] = json::array();
            for (auto const& l : g.labels())
            {
                json lj;
                lj["id"]  = l.id;
                std::string kind_str{"out"};
                if (l.kind == LabelKind::In) { kind_str = "in"; }
                lj["kind"]  = kind_str;
                lj["name"]  = l.name;
                lj["pos"]   = json::array({ l.pos.x, l.pos.y });
                lj["color"] = format_rgba(l.color);
                doc["labels"].push_back(lj);
            }
        }

        return doc;
    }

    std::string serialize_bundle(std::vector<PipelineRef> const& pipelines)
    {
        json doc;
        doc["version"]   = format_version;
        doc["pipelines"] = json::array();
        for (auto const& p : pipelines)
        {
            if (p.graph == nullptr)
            {
                continue;
            }
            doc["pipelines"].push_back(write_pipeline_body(*p.graph, p.name));
        }
        return doc.dump(2);
    }

    std::string serialize(Graph const& g, std::string const& name)
    {
        return serialize_bundle({ PipelineRef{ name, &g } });
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
            json const& id_json = node_json.at("id");
            if (not id_json.is_number_unsigned()
                or id_json.get<NodeId>() == invalid_node_id)
            {
                diags.push_back(schema_error(
                    "node 'id' must be a positive integer"));
                return false;
            }
            out.id    = id_json.get<NodeId>();
            out.type  = node_json.at("type").get<std::string>();
            out.name  = node_json.value("name",  std::string{});
            out.stage = node_json.value("stage", std::string{});
            out.note  = node_json.value("note",  std::string{});

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
                    if (auto val_it = attr_json.find("value"); val_it != attr_json.end())
                    {
                        a.value = decode_attr_value(*val_it);
                    }
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
            json const& id_json = link_json.at("id");
            if (not id_json.is_number_unsigned()
                or id_json.get<LinkId>() == invalid_link_id)
            {
                diags.push_back(schema_error(
                    "link 'id' must be a positive integer"));
                return false;
            }
            out.id = id_json.get<LinkId>();
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
                d.kind      = Diagnostic::Kind::AttributeMissing;
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
                d.kind      = Diagnostic::Kind::AttributeDrift;
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
                d.kind      = Diagnostic::Kind::AttributeAdded;
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
                d.kind    = Diagnostic::Kind::UnknownStageReference;
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
                        d.kind      = Diagnostic::Kind::UnknownStageReference;
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

    Pipeline parse_pipeline_body(json const& doc, NodeRegistry const& registry)
    {
        Pipeline result;
        NodeId max_node_id = 0;
        LinkId max_link_id = 0;

        if (auto it = doc.find("name"); it != doc.end() and it->is_string())
        {
            result.name = it->get<std::string>();
        }

        if (auto it = doc.find("meta"); it != doc.end() and it->is_object())
        {
            for (auto const& [k, v] : it->items())
            {
                if (v.is_string())
                {
                    result.graph.meta()[k] = v.get<std::string>();
                }
                else
                {
                    result.graph.meta()[k] = v.dump();
                }
            }
        }

        if (auto it = doc.find("default_mode"); it != doc.end() and it->is_string())
        {
            result.graph.set_default_mode_name(it->get<std::string>());
        }

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
                try
                {
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
                }
                catch (json::exception const& e)
                {
                    result.diagnostics.push_back(schema_error(
                        std::string("stage parse error: ") + e.what()));
                    continue;
                }
                if (not result.graph.add_stage(s))
                {
                    Diagnostic d;
                    d.kind    = Diagnostic::Kind::DuplicateStageName;
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

                // Old saves carry labels as Node entries. Convert them
                // to first-class Label entities so links pointing at
                // them resolve correctly via Graph::find_label.
                if (node.type == "label_in" or node.type == "label_out")
                {
                    Label l;
                    l.id   = node.id;
                    l.kind = LabelKind::Out;
                    if (node.type == "label_in") { l.kind = LabelKind::In; }
                    Attribute const* na = node.find_attr("name");
                    if (na != nullptr) { l.name = na->value; }
                    l.pos = node.pos;
                    if (node.id > max_node_id) { max_node_id = node.id; }
                    if (not result.graph.insert_label(l))
                    {
                        result.diagnostics.push_back(schema_error(
                            "duplicate id " + std::to_string(node.id)
                            + " (migrating label-as-node)"));
                    }
                    continue;
                }

                NodeType const* spec = registry.find(node.type);
                if (spec == nullptr)
                {
                    Diagnostic d;
                    d.kind    = Diagnostic::Kind::UnknownNodeType;
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
                    d.kind    = Diagnostic::Kind::DuplicateNodeId;
                    d.message = "duplicate node id " + std::to_string(node.id);
                    d.node_id = node.id;
                    result.diagnostics.push_back(d);
                }
            }
        }

        // Labels must load before links: a link whose endpoint is a
        // first-class label would otherwise be dropped as orphaned.
        if (auto it = doc.find("labels"); it != doc.end() and it->is_array())
        {
            for (auto const& lj : *it)
            {
                Label l;
                if (not lj.contains("id"))
                {
                    result.diagnostics.push_back(schema_error("label missing 'id'"));
                    continue;
                }
                try
                {
                    json const& id_json = lj.at("id");
                    if (not id_json.is_number_unsigned()
                        or id_json.get<LabelId>() == invalid_label_id)
                    {
                        result.diagnostics.push_back(schema_error(
                            "label 'id' must be a positive integer"));
                        continue;
                    }
                    l.id = id_json.get<LabelId>();
                    std::string kind_str{"in"};
                    if (auto kit = lj.find("kind"); kit != lj.end() and kit->is_string())
                    {
                        kind_str = kit->get<std::string>();
                    }
                    l.kind = LabelKind::In;
                    if (kind_str == "out") { l.kind = LabelKind::Out; }
                    if (auto nit = lj.find("name"); nit != lj.end() and nit->is_string())
                    {
                        l.name = nit->get<std::string>();
                    }
                    if (auto pit = lj.find("pos"); pit != lj.end() and pit->is_array() and pit->size() == 2)
                    {
                        l.pos.x = pit->at(0).get<float>();
                        l.pos.y = pit->at(1).get<float>();
                    }
                    if (auto cit = lj.find("color"); cit != lj.end() and cit->is_string())
                    {
                        auto parsed = parse_rgba(cit->get<std::string>());
                        if (parsed.has_value())
                        {
                            l.color = *parsed;
                        }
                        else
                        {
                            result.diagnostics.push_back(schema_error(
                                "label '" + l.name + "' has malformed color"));
                        }
                    }
                }
                catch (json::exception const& e)
                {
                    result.diagnostics.push_back(schema_error(
                        std::string("label parse error: ") + e.what()));
                    continue;
                }
                if (max_node_id < l.id)
                {
                    max_node_id = l.id;
                }
                if (not result.graph.insert_label(l))
                {
                    result.diagnostics.push_back(schema_error(
                        "duplicate label id " + std::to_string(l.id)));
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

                Label const* from_label = result.graph.find_label(link.from.node);
                Label const* to_label   = result.graph.find_label(link.to.node);
                Node const* from_node = nullptr;
                Node const* to_node   = nullptr;
                if (from_label == nullptr)
                {
                    from_node = result.graph.find_node(link.from.node);
                }
                if (to_label == nullptr)
                {
                    to_node = result.graph.find_node(link.to.node);
                }

                bool const from_known = (from_label != nullptr or from_node != nullptr);
                bool const to_known   = (to_label != nullptr or to_node != nullptr);
                if (not from_known or not to_known)
                {
                    Diagnostic d;
                    d.kind    = Diagnostic::Kind::LinkOrphanedNode;
                    d.message = "link " + std::to_string(link.id) + " references unknown node";
                    d.link_id = link.id;
                    result.diagnostics.push_back(d);
                    continue;
                }

                // Migrate old saves: pre-Label, the label-as-node had
                // attrs "in"/"out". Rewrite to the unified "pin" name.
                if (from_label != nullptr and link.from.attr != label_pin_name)
                {
                    link.from.attr = label_pin_name;
                }
                if (to_label != nullptr and link.to.attr != label_pin_name)
                {
                    link.to.attr = label_pin_name;
                }

                Attribute const* from_attr = nullptr;
                Attribute const* to_attr   = nullptr;
                if (from_node != nullptr) { from_attr = from_node->find_attr(link.from.attr); }
                if (to_node   != nullptr) { to_attr   = to_node->find_attr(link.to.attr);   }

                if ((from_node != nullptr and from_attr == nullptr)
                    or (to_node != nullptr and to_attr == nullptr))
                {
                    Diagnostic d;
                    d.kind    = Diagnostic::Kind::LinkOrphanedAttribute;
                    d.message = "link " + std::to_string(link.id) + " references unknown attribute";
                    d.link_id = link.id;
                    result.diagnostics.push_back(d);
                    continue;
                }

                // Type-mismatch check applies only when both ends are
                // real Node pins; label pins are wildcards.
                bool type_mismatch = false;
                if (from_attr != nullptr and to_attr != nullptr)
                {
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
                }

                if (type_mismatch)
                {
                    Diagnostic d;
                    d.kind    = Diagnostic::Kind::LinkTypeMismatch;
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
                    d.kind    = Diagnostic::Kind::DuplicateLinkId;
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
                try
                {
                    m.name = mode_json.at("name").get<std::string>();

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
                                d.kind    = Diagnostic::Kind::OrphanModeReference;
                                d.message = "mode profile '" + m.name + "' references unknown node "
                                            + std::to_string(nid);
                                d.node_id = nid;
                                result.diagnostics.push_back(d);
                            }
                            m.per_node[nid] = label;
                        }
                    }
                }
                catch (json::exception const& e)
                {
                    result.diagnostics.push_back(schema_error(
                        std::string("mode profile parse error: ") + e.what()));
                    continue;
                }

                if (not result.graph.add_mode_profile(m))
                {
                    Diagnostic d;
                    d.kind    = Diagnostic::Kind::DuplicateProfileName;
                    d.message = "duplicate mode profile name '" + m.name + "'";
                    result.diagnostics.push_back(d);
                }
            }
        }

        check_stage_references(result.graph, result.diagnostics);

        AnnotationId max_annotation_id = invalid_annotation_id;
        if (auto it = doc.find("annotations"); it != doc.end() and it->is_array())
        {
            for (auto const& an : *it)
            {
                Annotation a;
                if (not an.contains("id"))
                {
                    result.diagnostics.push_back(schema_error("annotation missing 'id'"));
                    continue;
                }
                try
                {
                    json const& id_json = an.at("id");
                    if (not id_json.is_number_unsigned()
                        or id_json.get<AnnotationId>() == invalid_annotation_id)
                    {
                        result.diagnostics.push_back(schema_error(
                            "annotation 'id' must be a positive integer"));
                        continue;
                    }
                    a.id = id_json.get<AnnotationId>();
                    if (auto pit = an.find("pos"); pit != an.end() and pit->is_array() and pit->size() == 2)
                    {
                        a.pos.x = pit->at(0).get<float>();
                        a.pos.y = pit->at(1).get<float>();
                    }
                    if (auto sit = an.find("size"); sit != an.end() and sit->is_array() and sit->size() == 2)
                    {
                        a.size.x = sit->at(0).get<float>();
                        a.size.y = sit->at(1).get<float>();
                    }
                    if (auto cit = an.find("color"); cit != an.end() and cit->is_string())
                    {
                        auto parsed = parse_rgba(cit->get<std::string>());
                        if (parsed.has_value())
                        {
                            a.color = *parsed;
                        }
                    }
                    if (auto tit = an.find("text"); tit != an.end() and tit->is_string())
                    {
                        a.text = tit->get<std::string>();
                    }
                }
                catch (json::exception const& e)
                {
                    result.diagnostics.push_back(schema_error(
                        std::string("annotation parse error: ") + e.what()));
                    continue;
                }
                if (a.id > max_annotation_id)
                {
                    max_annotation_id = a.id;
                }
                if (not result.graph.insert_annotation(a))
                {
                    result.diagnostics.push_back(schema_error(
                        "duplicate annotation id " + std::to_string(a.id)));
                }
            }
        }

        result.graph.reserve_ids_above(max_node_id, max_link_id);
        result.graph.reserve_annotation_id_above(max_annotation_id);
        // Enforce per-cluster color invariant on load. Files written
        // by older code (no `color` field), externally authored
        // packs, or v2->v3 migrated label-as-node entries can land
        // with mismatched colors in the same cluster; this normalises
        // them using the first-by-id label's color and surfaces an
        // info diagnostic when anything actually changed so callers
        // can let the user know.
        std::size_t const repaired = result.graph.repair_label_clusters();
        if (repaired > 0)
        {
            Diagnostic d;
            d.kind    = Diagnostic::Kind::LabelClusterRepaired;
            d.message = "normalised label color across "
                      + std::to_string(repaired)
                      + " label(s) to enforce per-cluster color";
            result.diagnostics.push_back(d);
        }
        return result;
    }

    BundleLoadResult deserialize_bundle(std::string_view     jsonstr,
                                         NodeRegistry const& registry)
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

        if (not doc.is_object())
        {
            throw std::runtime_error("malformed document: top level is not an object");
        }
        auto version_it = doc.find("version");
        if (version_it == doc.end() or not version_it->is_number_integer())
        {
            throw std::runtime_error(
                "malformed document: 'version' missing or not an integer");
        }
        int version = version_it->get<int>();
        // Loader accepts versions in [min_supported_version,
        // format_version]. v2 -> v3: labels were promoted from
        // label_in/label_out node entries to first-class Label
        // entities; the migration in parse_pipeline_body picks up
        // label_in/label_out node entries from v2 saves regardless.
        if (version < min_supported_version or version > format_version)
        {
            throw std::runtime_error(
                "unsupported V2 format version " + std::to_string(version)
                + " (supported: " + std::to_string(min_supported_version)
                + ".." + std::to_string(format_version) + ")");
        }

        BundleLoadResult result;

        auto pipelines_it = doc.find("pipelines");
        if (pipelines_it != doc.end())
        {
            if (not pipelines_it->is_array())
            {
                throw std::runtime_error(
                    "malformed document: 'pipelines' is not an array");
            }
            for (auto const& pipeline_json : *pipelines_it)
            {
                if (not pipeline_json.is_object())
                {
                    result.diagnostics.push_back(schema_error("bundle 'pipelines' entry is not an object"));
                    continue;
                }
                result.pipelines.push_back(parse_pipeline_body(pipeline_json, registry));
            }
            return result;
        }

        // Unwrapped shape: top-level doc is itself a single pipeline.
        // Preserved so legacy single-pipeline files still load.
        result.pipelines.push_back(parse_pipeline_body(doc, registry));
        return result;
    }

    LoadResult deserialize(std::string_view jsonstr, NodeRegistry const& registry)
    {
        BundleLoadResult bundle = deserialize_bundle(jsonstr, registry);
        LoadResult       result;
        // Top-level diagnostics surface alongside the first pipeline's
        // so single-pipeline callers see schema errors against the
        // bundle wrapper.
        result.diagnostics = std::move(bundle.diagnostics);
        if (not bundle.pipelines.empty())
        {
            Pipeline& first = bundle.pipelines.front();
            result.graph    = std::move(first.graph);
            result.diagnostics.insert(result.diagnostics.end(),
                                       first.diagnostics.begin(),
                                       first.diagnostics.end());
        }
        return result;
    }

    // ---- Registry (engine's node-type catalog) ----

    std::string serialize_registry(NodeRegistry const& reg)
    {
        json doc;
        doc["version"] = registry_format_version;
        doc["types"]   = json::array();

        // Sort by type name so output is stable across runs.
        auto types = reg.all();
        std::sort(types.begin(), types.end(),
                  [](NodeType const* a, NodeType const* b) { return a->type < b->type; });

        for (auto const* nt : types)
        {
            json type_json;
            type_json["type"] = nt->type;
            auto const lib = reg.library_of(nt->type);
            if (not lib.empty())
            {
                type_json["library"] = std::string{lib};
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

        if (not doc.is_object())
        {
            throw std::runtime_error("malformed document: top level is not an object");
        }
        auto version_it = doc.find("version");
        if (version_it == doc.end() or not version_it->is_number_integer())
        {
            throw std::runtime_error(
                "malformed document: 'version' missing or not an integer");
        }
        int version = version_it->get<int>();
        if (version < registry_min_supported_version
            or version > registry_format_version)
        {
            throw std::runtime_error(
                "unsupported registry format version " + std::to_string(version)
                + " (supported: " + std::to_string(registry_min_supported_version)
                + ".." + std::to_string(registry_format_version) + ")");
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

                NodeType    nt;
                std::string library;
                try
                {
                    nt.type     = type_json.at("type").get<std::string>();
                    library     = type_json.value("library",  std::string{});
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

                if (not result.registry.add(std::move(library), nt))
                {
                    Diagnostic d;
                    d.kind    = Diagnostic::Kind::DuplicateTypeName;
                    d.message = "duplicate node type '" + nt.type + "'";
                    result.diagnostics.push_back(d);
                }
            }
        }

        return result;
    }
}
