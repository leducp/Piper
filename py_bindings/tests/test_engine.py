"""Tests for piper.engine bindings.

Run with: python -m unittest py_bindings.tests.test_engine
"""

import unittest

import piper
import piper.engine as eng


def _make_stage(name: str) -> "piper.Stage":
    s = piper.Stage()
    s.name = name
    return s


def _build_constant_to_probe(value: float):
    """Single-stage chain: constant<float> -> probe<float>."""
    nr = piper.NodeRegistry()
    piper.register_builtin_nodes(nr)

    g = piper.Graph()
    g.add_stage(_make_stage("control"))

    cf = nr.find("constant<float>")
    pr = nr.find("external_output<float>")
    src_id   = g.add_node(cf, "src",   "control", piper.Point(0, 0))
    probe_id = g.add_node(pr, "probe", "control", piper.Point(1, 0))
    g.set_attr_value(src_id, "value", str(value))
    g.add_link(piper.PinRef(src_id, "out"),
               piper.PinRef(probe_id, "in"),
               "float")
    return g, probe_id


class TestEngineDriveOnly(unittest.TestCase):
    def test_build_and_tick_constant_to_probe(self):
        g, probe_id = _build_constant_to_probe(3.5)

        sr = eng.StepRegistry()
        eng.register_builtin_steps(sr)

        e = eng.Engine()
        result = e.build(g, sr)
        self.assertTrue(result.ok, msg=[d.message for d in result.diagnostics])

        e.tick("control")

        probe = e.step_for(probe_id)
        self.assertIsNotNone(probe)
        self.assertAlmostEqual(probe.read_input_float("in"), 3.5, places=5)

    def test_play_iterates_all_stages(self):
        g, probe_id = _build_constant_to_probe(1.25)
        sr = eng.StepRegistry()
        eng.register_builtin_steps(sr)
        e = eng.Engine()
        self.assertTrue(e.build(g, sr).ok)

        for _ in range(10):
            e.play()

        probe = e.step_for(probe_id)
        self.assertAlmostEqual(probe.read_input_float("in"), 1.25, places=5)
        self.assertEqual(e.stages(), ["control"])

    def test_unknown_factory_emits_diagnostic(self):
        g, _ = _build_constant_to_probe(0.0)
        sr = eng.StepRegistry()  # intentionally empty -- no factories
        e = eng.Engine()
        result = e.build(g, sr)
        self.assertFalse(result.ok)
        kinds = {d.kind for d in result.diagnostics}
        self.assertIn(eng.BuildDiagnostic.Kind.UnknownStepFactory, kinds)


class TestPythonAuthoredStep(unittest.TestCase):
    def test_python_step_runs(self):
        # A doubler implemented entirely in Python.
        class Doubler(eng.Step):
            def declare_io(self):
                self.declare_input_float("in")

            def compute(self, _stage):
                # The doubler doesn't publish an output here; this test
                # only verifies that compute() is called and can read
                # its input. A more elaborate output path is exercised
                # below.
                self.observed = self.read_input_float("in")

        # Build: constant<float>(2.0) -> doubler.
        nr = piper.NodeRegistry()
        piper.register_builtin_nodes(nr)
        # Author metadata for the doubler so the editor side accepts it.
        doubler_meta = piper.NodeType()
        doubler_meta.type     = "py_doubler"
        doubler_meta.attributes = [
            _attr("in", "float", piper.AttributeSpec.Role.Input),
        ]
        nr.add(doubler_meta)

        g = piper.Graph()
        g.add_stage(_make_stage("control"))
        cf = nr.find("constant<float>")
        src_id     = g.add_node(cf,           "src",     "control", piper.Point(0, 0))
        doubler_id = g.add_node(doubler_meta, "doubler", "control", piper.Point(1, 0))
        g.set_attr_value(src_id, "value", "2.0")
        g.add_link(piper.PinRef(src_id, "out"),
                   piper.PinRef(doubler_id, "in"),
                   "float")

        sr = eng.StepRegistry()
        eng.register_builtin_steps(sr)
        eng.register_step_type_py(sr, "py_doubler", Doubler)

        e = eng.Engine()
        result = e.build(g, sr)
        self.assertTrue(result.ok, msg=[d.message for d in result.diagnostics])

        e.tick("control")

        step = e.step_for(doubler_id)
        self.assertIsNotNone(step)
        # The Doubler stored the observed input on the Python instance.
        self.assertAlmostEqual(step.observed, 2.0, places=5)


    def test_python_step_publishes_output_to_downstream(self):
        # Python-authored counter feeds a C++ probe.
        class Counter(eng.Step):
            def declare_io(self):
                self.declare_output_int("out")
                self.value = 0

            def compute(self, _stage):
                self.value += 1
                self.set_output_int("out", self.value)

        nr = piper.NodeRegistry()
        piper.register_builtin_nodes(nr)
        counter_meta = piper.NodeType()
        counter_meta.type = "py_counter"
        counter_meta.attributes = [
            _attr("out", "int32_t", piper.AttributeSpec.Role.Output),
        ]
        nr.add(counter_meta)

        g = piper.Graph()
        g.add_stage(_make_stage("control"))
        ctr_id   = g.add_node(counter_meta,
                              "ctr",   "control", piper.Point(0, 0))
        probe_id = g.add_node(nr.find("external_output<int32_t>"),
                              "probe", "control", piper.Point(1, 0))
        g.add_link(piper.PinRef(ctr_id, "out"),
                   piper.PinRef(probe_id, "in"),
                   "int")

        sr = eng.StepRegistry()
        eng.register_builtin_steps(sr)
        eng.register_step_type_py(sr, "py_counter", Counter)

        e = eng.Engine()
        result = e.build(g, sr)
        self.assertTrue(result.ok, msg=[d.message for d in result.diagnostics])

        for _ in range(5):
            e.tick("control")

        probe = e.step_for(probe_id)
        self.assertEqual(probe.read_input_int("in"), 5)


class TestExternalIO(unittest.TestCase):
    def test_set_input_flows_to_output(self):
        # Mimics the HAL scenario: external_input -> low_pass -> external_output.
        nr = piper.NodeRegistry()
        piper.register_builtin_nodes(nr)

        g = piper.Graph()
        g.add_stage(_make_stage("control"))

        in_t  = nr.find("external_input<float>")
        out_t = nr.find("external_output<float>")
        lp_t  = nr.find("low_pass<float>")

        in_id  = g.add_node(in_t,  "ipc_in",  "control", piper.Point(0, 0))
        lp_id  = g.add_node(lp_t,  "filter",  "control", piper.Point(1, 0))
        out_id = g.add_node(out_t, "ipc_out", "control", piper.Point(2, 0))

        g.set_attr_value(in_id,  "name",   "target")
        g.set_attr_value(lp_id,  "cutoff", "100.0")
        g.set_attr_value(out_id, "name",   "measured")
        g.add_link(piper.PinRef(in_id,  "out"), piper.PinRef(lp_id,  "in"), "float")
        g.add_link(piper.PinRef(lp_id,  "out"), piper.PinRef(out_id, "in"), "float")

        sr = eng.StepRegistry()
        eng.register_builtin_steps(sr)

        e = eng.Engine()
        result = e.build(g, sr)
        self.assertTrue(result.ok, msg=[d.message for d in result.diagnostics])

        # Resolve handles ONCE -- HAL hot path uses the cached pointers.
        target = e.input_float("target")
        meas   = e.output_float("measured")
        self.assertIsNotNone(target)
        self.assertIsNotNone(meas)

        target.set(0.75)
        for _ in range(1000):
            e.play()
        self.assertAlmostEqual(meas.get(), 0.75, places=3)

    def test_unknown_name_returns_none(self):
        nr = piper.NodeRegistry()
        piper.register_builtin_nodes(nr)
        g = piper.Graph()
        g.add_stage(_make_stage("control"))
        in_id = g.add_node(nr.find("external_input<float>"),
                           "anon", "control", piper.Point(0, 0))
        g.set_attr_value(in_id, "name", "valid")

        sr = eng.StepRegistry()
        eng.register_builtin_steps(sr)
        e = eng.Engine()
        self.assertTrue(e.build(g, sr).ok)

        self.assertIsNone(e.input_float("does_not_exist"))
        self.assertIsNotNone(e.input_float("valid"))
        # Wrong type also returns None.
        self.assertIsNone(e.input_int("valid"))


def _attr(name, dtype, role, default=""):
    a = piper.AttributeSpec()
    a.name          = name
    a.data_type     = dtype
    a.role          = role
    a.default_value = default
    return a


if __name__ == "__main__":
    unittest.main()
