#include <nanobind/nanobind.h>
#include <nanobind/operators.h>
#include <nanobind/stl/map.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/vector.h>

#include "piper/attribute.h"
#include "piper/builtin_nodes.h"
#include "piper/diagnostic.h"
#include "piper/graph.h"
#include "piper/link.h"
#include "piper/mode_profile.h"
#include "piper/node.h"
#include "piper/node_type.h"
#include "piper/registry.h"
#include "piper/serialize_v2.h"
#include "piper/stage.h"

namespace nb = nanobind;
using namespace piper;

void bind_engine(nb::module_ m);

NB_MODULE(piper, m)
{
    m.doc() = "Piper -- visual designer for control-system pipelines.";
    m.attr("__version__")        = "0.1.0";
    m.attr("invalid_node_id")    = invalid_node_id;
    m.attr("invalid_link_id")    = invalid_link_id;

    // ---- Diagnostic + nested Kind ----
    auto diag_class = nb::class_<Diagnostic>(m, "Diagnostic");
    nb::enum_<Diagnostic::Kind>(diag_class, "Kind")
        .value("SchemaError",            Diagnostic::Kind::SchemaError)
        .value("DuplicateNodeId",        Diagnostic::Kind::DuplicateNodeId)
        .value("DuplicateLinkId",        Diagnostic::Kind::DuplicateLinkId)
        .value("DuplicateStageName",     Diagnostic::Kind::DuplicateStageName)
        .value("DuplicateProfileName",   Diagnostic::Kind::DuplicateProfileName)
        .value("DuplicateTypeName",      Diagnostic::Kind::DuplicateTypeName)
        .value("UnknownNodeType",        Diagnostic::Kind::UnknownNodeType)
        .value("AttributeMissing",       Diagnostic::Kind::AttributeMissing)
        .value("AttributeAdded",         Diagnostic::Kind::AttributeAdded)
        .value("AttributeDrift",         Diagnostic::Kind::AttributeDrift)
        .value("LinkOrphanedNode",       Diagnostic::Kind::LinkOrphanedNode)
        .value("LinkOrphanedAttribute",  Diagnostic::Kind::LinkOrphanedAttribute)
        .value("LinkTypeMismatch",       Diagnostic::Kind::LinkTypeMismatch)
        .value("OrphanModeReference",    Diagnostic::Kind::OrphanModeReference)
        .value("UnknownStageReference",  Diagnostic::Kind::UnknownStageReference);
    diag_class
        .def(nb::init<>())
        .def_rw("kind",      &Diagnostic::kind)
        .def_rw("message",   &Diagnostic::message)
        .def_rw("node_id",   &Diagnostic::node_id)
        .def_rw("attr_name", &Diagnostic::attr_name)
        .def_rw("link_id",   &Diagnostic::link_id);

    // ---- Point ----
    nb::class_<Point>(m, "Point")
        .def(nb::init<>())
        .def("__init__", [](Point* self, float x, float y) { new (self) Point{x, y}; })
        .def_rw("x", &Point::x)
        .def_rw("y", &Point::y)
        .def(nb::self == nb::self)
        .def(nb::self != nb::self);

    // ---- AttributeSpec + Role ----
    auto spec_cls = nb::class_<AttributeSpec>(m, "AttributeSpec");
    nb::enum_<AttributeSpec::Role>(spec_cls, "Role")
        .value("Input",  AttributeSpec::Role::Input)
        .value("Output", AttributeSpec::Role::Output)
        .value("Member", AttributeSpec::Role::Member);
    spec_cls
        .def(nb::init<>())
        .def_rw("name",          &AttributeSpec::name)
        .def_rw("data_type",     &AttributeSpec::data_type)
        .def_rw("role",          &AttributeSpec::role)
        .def_rw("default_value", &AttributeSpec::default_value);

    // ---- Attribute ----
    nb::class_<Attribute>(m, "Attribute")
        .def(nb::init<>())
        .def_rw("name",      &Attribute::name)
        .def_rw("data_type", &Attribute::data_type)
        .def_rw("role",      &Attribute::role)
        .def_rw("value",     &Attribute::value)
        .def_rw("stages",    &Attribute::stages);

    // ---- NodeType ----
    nb::class_<NodeType>(m, "NodeType")
        .def(nb::init<>())
        .def_rw("type",       &NodeType::type)
        .def_rw("help",       &NodeType::help)
        .def_rw("category",   &NodeType::category)
        .def_rw("attributes", &NodeType::attributes);

    // ---- PinRef ----
    nb::class_<PinRef>(m, "PinRef")
        .def(nb::init<>())
        .def("__init__",
             [](PinRef* self, NodeId node, std::string attr)
             {
                 new (self) PinRef{ node, std::move(attr) };
             },
             nb::arg("node"), nb::arg("attr"))
        .def_rw("node", &PinRef::node)
        .def_rw("attr", &PinRef::attr)
        .def(nb::self == nb::self)
        .def(nb::self != nb::self);

    // ---- Node ----
    nb::class_<Node>(m, "Node")
        .def(nb::init<>())
        .def_rw("id",    &Node::id)
        .def_rw("type",  &Node::type)
        .def_rw("name",  &Node::name)
        .def_rw("stage", &Node::stage)
        .def_rw("pos",   &Node::pos)
        .def_rw("attrs", &Node::attrs)
        .def("find_attr",
             [](Node const& n, std::string_view name) -> Attribute const*
             {
                 return n.find_attr(name);
             },
             nb::rv_policy::reference_internal);

    // ---- Link ----
    nb::class_<Link>(m, "Link")
        .def(nb::init<>())
        .def_rw("id",        &Link::id)
        .def_rw("from_",     &Link::from)
        .def_rw("to",        &Link::to)
        .def_rw("data_type", &Link::data_type);

    // ---- Stage ----
    nb::class_<Stage>(m, "Stage")
        .def(nb::init<>())
        .def_rw("name",  &Stage::name)
        .def_rw("color", &Stage::color);

    // ---- ModeProfile ----
    nb::class_<ModeProfile>(m, "ModeProfile")
        .def(nb::init<>())
        .def_rw("name",     &ModeProfile::name)
        .def_rw("per_node", &ModeProfile::per_node);

    // ---- NodeRegistry ----
    nb::class_<NodeRegistry>(m, "NodeRegistry")
        .def(nb::init<>())
        .def("add",
             [](NodeRegistry& r, NodeType const& nt) { return r.add(nt); },
             nb::arg("node_type"))
        .def("add",
             [](NodeRegistry& r, std::string library, NodeType nt)
             {
                 return r.add(std::move(library), std::move(nt));
             },
             nb::arg("library"), nb::arg("node_type"))
        .def("find",
             [](NodeRegistry const& r, std::string_view name) -> NodeType const*
             {
                 return r.find(name);
             },
             nb::rv_policy::reference_internal,
             nb::arg("name"))
        .def("all",
             [](NodeRegistry const& r)
             {
                 std::vector<NodeType> out;
                 auto const ptrs = r.all();
                 out.reserve(ptrs.size());
                 for (auto const* p : ptrs)
                 {
                     out.push_back(*p);
                 }
                 return out;
             })
        .def("library_of",
             [](NodeRegistry const& r, std::string_view name) -> std::string {
                 return std::string{r.library_of(name)};
             },
             nb::arg("type_name"));

    m.def("register_builtin_nodes", &register_builtin_nodes, nb::arg("registry"),
          "Register Piper's bundled node types into the given registry.");

    // ---- Graph ----
    nb::class_<Graph>(m, "Graph")
        .def(nb::init<>())
        .def("add_node", &Graph::add_node,
             nb::arg("node_type"), nb::arg("name"),
             nb::arg("stage"),     nb::arg("pos"))
        .def("insert_node",  &Graph::insert_node, nb::arg("node"))
        .def("remove_node",  &Graph::remove_node, nb::arg("id"))
        .def("add_link",     &Graph::add_link,
             nb::arg("from"), nb::arg("to"), nb::arg("data_type"))
        .def("insert_link",  &Graph::insert_link, nb::arg("link"))
        .def("remove_link",  &Graph::remove_link, nb::arg("id"))
        .def("set_attr_value",
             [](Graph& g, NodeId id, std::string const& name, std::string const& value)
             {
                 return g.set_attr_value(id, name, value);
             },
             nb::arg("node_id"), nb::arg("attr_name"), nb::arg("value"))
        .def("set_attr_stages",
             [](Graph& g, NodeId id, std::string const& name,
                std::vector<std::string> const& stages)
             {
                 return g.set_attr_stages(id, name, stages);
             },
             nb::arg("node_id"), nb::arg("attr_name"), nb::arg("stages"))
        .def("move_node",       &Graph::move_node)
        .def("set_node_stage",  &Graph::set_node_stage)
        .def("rename_node",     &Graph::rename_node)
        .def("add_stage",       &Graph::add_stage)
        .def("remove_stage",
             [](Graph& g, std::string const& name) { g.remove_stage(name); },
             nb::arg("name"))
        .def("add_mode_profile",    &Graph::add_mode_profile)
        .def("remove_mode_profile",
             [](Graph& g, std::string const& name) { g.remove_mode_profile(name); },
             nb::arg("name"))
        .def("set_node_mode_label",
             [](Graph& g, std::string const& profile, NodeId id, std::string const& label)
             {
                 return g.set_node_mode_label(profile, id, label);
             },
             nb::arg("profile"), nb::arg("node_id"), nb::arg("label"))
        .def("nodes",  [](Graph const& g) { return g.nodes(); })
        .def("links",  [](Graph const& g) { return g.links(); })
        .def("stages", [](Graph const& g) { return g.stages(); })
        .def("mode_profiles", [](Graph const& g) { return g.mode_profiles(); })
        .def("default_mode_name",
             [](Graph const& g) { return g.default_mode_name(); })
        .def("set_default_mode_name",
             [](Graph& g, std::string name) { g.set_default_mode_name(std::move(name)); },
             nb::arg("name"))
        .def("meta",     [](Graph const& g) { return g.meta(); })
        .def("set_meta",
             [](Graph& g, std::map<std::string, std::string> const& m)
             {
                 g.meta() = m;
             },
             nb::arg("meta"))
        .def("find_node",
             [](Graph const& g, NodeId id) -> Node const*
             {
                 return g.find_node(id);
             },
             nb::rv_policy::reference_internal,
             nb::arg("id"))
        .def("find_link",
             [](Graph const& g, LinkId id) -> Link const*
             {
                 return g.find_link(id);
             },
             nb::rv_policy::reference_internal,
             nb::arg("id"));

    // ---- piper.v2 submodule ----
    auto v2 = m.def_submodule("v2", "V2 file format serializer / deserializer.");
    v2.attr("format_version") = v2::format_version;

    nb::class_<v2::LoadResult>(v2, "LoadResult")
        .def(nb::init<>())
        .def_rw("graph",       &v2::LoadResult::graph)
        .def_rw("diagnostics", &v2::LoadResult::diagnostics);

    nb::class_<v2::Pipeline>(v2, "Pipeline")
        .def(nb::init<>())
        .def_rw("name",        &v2::Pipeline::name)
        .def_rw("graph",       &v2::Pipeline::graph)
        .def_rw("diagnostics", &v2::Pipeline::diagnostics);

    nb::class_<v2::BundleLoadResult>(v2, "BundleLoadResult")
        .def(nb::init<>())
        .def_rw("pipelines",   &v2::BundleLoadResult::pipelines)
        .def_rw("diagnostics", &v2::BundleLoadResult::diagnostics);

    v2.def("serialize",
           [](Graph const& g, std::string const& name)
           {
               return v2::serialize(g, name);
           },
           nb::arg("graph"), nb::arg("name") = std::string{});

    v2.def("deserialize",
           [](std::string_view text, NodeRegistry const& registry)
           {
               return v2::deserialize(text, registry);
           },
           nb::arg("text"), nb::arg("registry"));

    v2.def("deserialize_bundle",
           [](std::string_view text, NodeRegistry const& registry)
           {
               return v2::deserialize_bundle(text, registry);
           },
           nb::arg("text"), nb::arg("registry"));

    // Accepts either a list of v2.Pipeline (the natural shape returned
    // by deserialize_bundle) or a list of (name, Graph) tuples for
    // ad-hoc use. The Pipeline overload lets users round-trip a
    // BundleLoadResult without manually rebuilding tuples.
    v2.def("serialize_bundle",
           [](std::vector<v2::Pipeline> const& pipelines)
           {
               std::vector<v2::PipelineRef> refs;
               refs.reserve(pipelines.size());
               for (auto const& p : pipelines)
               {
                   refs.push_back({ p.name, &p.graph });
               }
               return v2::serialize_bundle(refs);
           },
           nb::arg("pipelines"));

    v2.def("serialize_bundle",
           [](std::vector<std::pair<std::string, Graph>> const& pipelines)
           {
               std::vector<v2::PipelineRef> refs;
               refs.reserve(pipelines.size());
               for (auto const& [name, g] : pipelines)
               {
                   refs.push_back({ name, &g });
               }
               return v2::serialize_bundle(refs);
           },
           nb::arg("pipelines"));

    // ---- piper.engine submodule ----
    auto engine_m = m.def_submodule("engine", "Pipeline runtime: builds an executable schedule from a piper.Graph.");
    bind_engine(engine_m);
}
