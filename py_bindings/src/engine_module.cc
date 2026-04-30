#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <nanobind/nanobind.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/string_view.h>
#include <nanobind/stl/vector.h>
#include <nanobind/trampoline.h>

#include "piper/engine/builtin_steps.h"
#include "piper/engine/diagnostic.h"
#include "piper/engine/engine.h"
#include "piper/engine/external_io.h"
#include "piper/engine/registry.h"
#include "piper/engine/stage.h"
#include "piper/engine/step.h"

namespace nb     = nanobind;
namespace eng    = piper::engine;
using namespace nb::literals;

// Trampoline so Python subclasses of piper.engine.Step participate
// in dispatch via NB_OVERRIDE_PURE / NB_OVERRIDE. Output storage for
// Python-authored steps lives on Step itself via the templated
// declare_output / set_output API; nothing
// type-specific is needed here.
struct PyStep : eng::Step
{
    NB_TRAMPOLINE(eng::Step, 2);

    void compute(eng::Stage current) override
    {
        NB_OVERRIDE_PURE(compute, current);
    }

    void declare_io() override
    {
        NB_OVERRIDE(declare_io);
    }
};

void bind_engine(nb::module_ m)
{
    auto bd_class = nb::class_<eng::BuildDiagnostic>(m, "BuildDiagnostic");
    nb::enum_<eng::BuildDiagnostic::Kind>(bd_class, "Kind")
        .value("UnknownStepFactory",   eng::BuildDiagnostic::Kind::UnknownStepFactory)
        .value("UnresolvedInput",      eng::BuildDiagnostic::Kind::UnresolvedInput)
        .value("TypeMismatchAtLink",   eng::BuildDiagnostic::Kind::TypeMismatchAtLink)
        .value("CycleDetected",        eng::BuildDiagnostic::Kind::CycleDetected)
        .value("UnknownStageOnPin",    eng::BuildDiagnostic::Kind::UnknownStageOnPin)
        .value("NodeNeverScheduled",   eng::BuildDiagnostic::Kind::NodeNeverScheduled)
        .value("StepDeclareIoFailed",  eng::BuildDiagnostic::Kind::StepDeclareIoFailed);
    bd_class
        .def(nb::init<>())
        .def_rw("kind",      &eng::BuildDiagnostic::kind)
        .def_rw("message",   &eng::BuildDiagnostic::message)
        .def_rw("node_id",   &eng::BuildDiagnostic::node_id)
        .def_rw("attr_name", &eng::BuildDiagnostic::attr_name)
        .def_rw("link_id",   &eng::BuildDiagnostic::link_id);

    nb::class_<eng::Engine::BuildResult>(m, "BuildResult")
        .def(nb::init<>())
        .def_rw("ok",          &eng::Engine::BuildResult::ok)
        .def_rw("diagnostics", &eng::Engine::BuildResult::diagnostics);

    nb::class_<eng::StepRegistry>(m, "StepRegistry")
        .def(nb::init<>())
        .def("size",  &eng::StepRegistry::size)
        .def("empty", &eng::StepRegistry::empty);

    // Stage handle. Python steps receive a Stage object in compute();
    // they typically read .name or compare .id against a precomputed
    // hash from piper.engine.hash_stage("...").
    nb::class_<eng::Stage>(m, "Stage")
        .def(nb::init<>())
        .def("__init__",
             [](eng::Stage* self, std::string_view name) { new (self) eng::Stage{name}; },
             "name"_a)
        .def_prop_ro("name",
                     [](eng::Stage const& s) { return std::string{s.name}; })
        .def_ro("id", &eng::Stage::id)
        .def("__eq__",
             [](eng::Stage const& a, eng::Stage const& b) { return a == b; })
        .def("__repr__",
             [](eng::Stage const& s) {
                 return std::string{"Stage('"} + std::string{s.name} + "')";
             });

    m.def("hash_stage",
          [](std::string_view name) { return eng::hash_stage(name); },
          "name"_a,
          "Compile-time-stable FNV-1a hash of a stage name. Compare "
          "this against Stage.id in step.compute() for fast dispatch.");

    // Step + trampoline. Typed read/write helpers are bound as
    // explicit per-type entry points because Step's templates can't
    // cross the language boundary directly.
    nb::class_<eng::Step, PyStep>(m, "Step")
        .def(nb::init<>())
        .def("declare_input_float",
             [](eng::Step& s, std::string_view name) { s.declare_input<float>(name); },
             "name"_a)
        .def("declare_input_int",
             [](eng::Step& s, std::string_view name) { s.declare_input<int32_t>(name); },
             "name"_a)
        .def("read_input_float",
             [](eng::Step const& s, std::string_view name) { return s.input<float>(name); },
             "name"_a)
        .def("read_input_int",
             [](eng::Step const& s, std::string_view name) { return s.input<int32_t>(name); },
             "name"_a)
        .def("read_output_float",
             [](eng::Step const& s, std::string_view name) { return s.output<float>(name); },
             "name"_a)
        .def("read_output_int",
             [](eng::Step const& s, std::string_view name) { return s.output<int32_t>(name); },
             "name"_a)
        .def("read_member",
             [](eng::Step const& s, std::string_view name) -> std::string {
                 return s.member(name);
             },
             "name"_a)
        // Engine-managed output declaration / write. The Step base
        // owns address-stable storage keyed by name, so Python-authored
        // steps publish outputs without exposing C++ slots.
        .def("declare_output_float",
             [](eng::Step& s, std::string_view name) {
                 s.declare_output<float>(name);
             },
             "name"_a)
        .def("declare_output_int",
             [](eng::Step& s, std::string_view name) {
                 s.declare_output<int32_t>(name);
             },
             "name"_a)
        .def("set_output_float",
             [](eng::Step& s, std::string_view name, float value) {
                 s.set_output<float>(name, value);
             },
             "name"_a, "value"_a)
        .def("set_output_int",
             [](eng::Step& s, std::string_view name, int value) {
                 s.set_output<int32_t>(name, value);
             },
             "name"_a, "value"_a);

    // External IO step types and their typed handles. Bound under
    // explicit names because templates do not cross the language
    // boundary; Python users call engine.input_float(name) /
    // engine.output_float(name) and then .set()/.get() on the result.
    nb::class_<eng::step::Input<float>, eng::Step>(m, "InputFloat")
        .def("set", &eng::step::Input<float>::set, "value"_a)
        .def("get", &eng::step::Input<float>::get);

    nb::class_<eng::step::Input<int32_t>, eng::Step>(m, "InputInt")
        .def("set", &eng::step::Input<int32_t>::set, "value"_a)
        .def("get", &eng::step::Input<int32_t>::get);

    nb::class_<eng::step::Output<float>, eng::Step>(m, "OutputFloat")
        .def("get", &eng::step::Output<float>::get);

    nb::class_<eng::step::Output<int32_t>, eng::Step>(m, "OutputInt")
        .def("get", &eng::step::Output<int32_t>::get);

    // Engine
    nb::class_<eng::Engine>(m, "Engine")
        .def(nb::init<>())
        .def("build", &eng::Engine::build, "graph"_a, "step_registry"_a)
        .def("tick",
             [](eng::Engine& self, std::string_view stage) {
                 eng::Stage const s{stage};   // hashes once per call
                 nb::gil_scoped_release rel;
                 self.tick(s);
             },
             "stage"_a)
        .def("play",
             [](eng::Engine& self) {
                 nb::gil_scoped_release rel;
                 self.play();
             })
        .def("input_float",
             [](eng::Engine& self, std::string_view name) {
                 return self.input<float>(name);
             },
             nb::rv_policy::reference_internal,
             "name"_a)
        .def("input_int",
             [](eng::Engine& self, std::string_view name) {
                 return self.input<int32_t>(name);
             },
             nb::rv_policy::reference_internal,
             "name"_a)
        .def("output_float",
             [](eng::Engine const& self, std::string_view name) {
                 return self.output<float>(name);
             },
             nb::rv_policy::reference_internal,
             "name"_a)
        .def("output_int",
             [](eng::Engine const& self, std::string_view name) {
                 return self.output<int32_t>(name);
             },
             nb::rv_policy::reference_internal,
             "name"_a)
        .def("step_for",
             [](eng::Engine& self, piper::NodeId id) -> eng::Step* {
                 return self.step_for(id);
             },
             nb::rv_policy::reference_internal,
             "node_id"_a)
        .def("stages",
             [](eng::Engine const& self) {
                 std::vector<std::string> out;
                 out.reserve(self.stages().size());
                 for (auto const& s : self.stages())
                 {
                     out.emplace_back(s.name);
                 }
                 return out;
             });

    m.def("register_builtin_steps", &eng::register_builtin_steps,
          "step_registry"_a,
          "Register the bundled Step factories into the given StepRegistry.");

    // Register a Python class as a Step factory. Each call to the
    // factory constructs a fresh Python instance; the resulting C++
    // Step is held by a shared_ptr whose deleter releases the Python
    // reference (which destroys the C++ side).
    m.def("register_step_type_py",
          [](eng::StepRegistry& sr, std::string type_name, nb::object py_class) {
              sr.add(std::move(type_name),
                     [py_class]() -> std::shared_ptr<eng::Step> {
                         nb::gil_scoped_acquire gil;
                         nb::object instance = py_class();
                         eng::Step* raw = nb::cast<eng::Step*>(instance);
                         // Holder keeps the Python ref alive; its
                         // deleter must acquire the GIL because the
                         // engine drops shared_ptrs from C++ paths
                         // that may not hold it.
                         auto holder = std::shared_ptr<nb::object>(
                             new nb::object(std::move(instance)),
                             [](nb::object* p) {
                                 nb::gil_scoped_acquire gil;
                                 delete p;
                             });
                         return std::shared_ptr<eng::Step>(holder, raw);
                     });
          },
          "step_registry"_a, "type_name"_a, "py_class"_a,
          "Register a Python Step subclass under the given type name. "
          "Each engine build that references this type instantiates a "
          "fresh Python instance.");
}
