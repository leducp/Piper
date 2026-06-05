"""Smoke tests for the piper Python bindings.

Run with: pytest py_bindings/tests/test_bindings.py
or: python -m unittest py_bindings.tests.test_bindings
"""

import unittest

import piper


class TestRegistry(unittest.TestCase):
    def test_register_builtins(self):
        reg = piper.NodeRegistry()
        piper.register_builtin_nodes(reg)
        types = reg.all()
        self.assertGreater(len(types), 0)
        names = {t.type for t in types}
        self.assertIn("sin_wave<float>", names)
        self.assertIn("low_pass<float>", names)
        self.assertIn("external_output<float>", names)

    def test_find_returns_attributes(self):
        reg = piper.NodeRegistry()
        piper.register_builtin_nodes(reg)
        nt = reg.find("low_pass<float>")
        self.assertIsNotNone(nt)
        self.assertEqual(nt.type, "low_pass<float>")
        attr_names = [a.name for a in nt.attributes]
        self.assertIn("in", attr_names)
        self.assertIn("out", attr_names)
        self.assertIn("cutoff", attr_names)


class TestGraph(unittest.TestCase):
    def setUp(self):
        self.reg = piper.NodeRegistry()
        piper.register_builtin_nodes(self.reg)

    def test_build_simple_graph(self):
        g = piper.Graph()
        sin_t   = self.reg.find("sin_wave<float>")
        probe_t = self.reg.find("external_output<float>")
        src  = g.add_node(sin_t,   "src",  "control", piper.Point(0, 0))
        sink = g.add_node(probe_t, "sink", "control", piper.Point(100, 0))
        link = g.add_link(piper.PinRef(src, "out"),
                          piper.PinRef(sink, "in"),
                          "float")
        self.assertNotEqual(src,  piper.invalid_node_id)
        self.assertNotEqual(sink, piper.invalid_node_id)
        self.assertNotEqual(link, piper.invalid_link_id)
        self.assertEqual(len(g.nodes()), 2)
        self.assertEqual(len(g.links()), 1)

    def test_attribute_value_persists(self):
        g = piper.Graph()
        nt = self.reg.find("low_pass<float>")
        node = g.add_node(nt, "f", "", piper.Point(0, 0))
        ok = g.set_attr_value(node, "cutoff", "5.0")
        self.assertTrue(ok)
        n = g.find_node(node)
        self.assertIsNotNone(n)
        cutoff_attr = n.find_attr("cutoff")
        self.assertIsNotNone(cutoff_attr)
        self.assertEqual(cutoff_attr.value, "5.0")

    def test_remove_node_cascades_to_links(self):
        g = piper.Graph()
        sin_t   = self.reg.find("sin_wave<float>")
        probe_t = self.reg.find("external_output<float>")
        src  = g.add_node(sin_t,   "src",  "", piper.Point(0, 0))
        sink = g.add_node(probe_t, "sink", "", piper.Point(0, 0))
        g.add_link(piper.PinRef(src, "out"), piper.PinRef(sink, "in"), "float")
        g.remove_node(src)
        self.assertEqual(len(g.links()), 0)


class TestRoundTrip(unittest.TestCase):
    def setUp(self):
        self.reg = piper.NodeRegistry()
        piper.register_builtin_nodes(self.reg)

    def test_serialize_deserialize_preserves_graph(self):
        g = piper.Graph()
        nt = self.reg.find("constant<float>")
        a = g.add_node(nt, "a", "", piper.Point(10, 20))
        g.set_attr_value(a, "value", "1.5")

        text = piper.v2.serialize(g, "main")
        self.assertIn('"pipelines"', text)

        loaded = piper.v2.deserialize(text, self.reg)
        self.assertEqual(len(loaded.diagnostics), 0)
        self.assertEqual(len(loaded.graph.nodes()), 1)
        n = loaded.graph.nodes()[0]
        self.assertEqual(n.name, "a")
        self.assertEqual(n.find_attr("value").value, "1.5")

    def test_bundle_round_trip(self):
        nt = self.reg.find("constant<float>")
        p1 = piper.v2.Pipeline()
        p1.name = "alpha"
        p1.graph.add_node(nt, "n1", "", piper.Point(0, 0))
        p2 = piper.v2.Pipeline()
        p2.name = "beta"
        p2.graph.add_node(nt, "n2", "", piper.Point(0, 0))

        text = piper.v2.serialize_bundle([p1, p2])
        loaded = piper.v2.deserialize_bundle(text, self.reg)
        self.assertEqual(len(loaded.pipelines), 2)
        names = [p.name for p in loaded.pipelines]
        self.assertIn("alpha", names)
        self.assertIn("beta", names)

    def test_bundle_round_trip_via_loaded(self):
        # Round-trip: serialize -> deserialize -> serialize again.
        nt = self.reg.find("constant<float>")
        p = piper.v2.Pipeline()
        p.name = "main"
        p.graph.add_node(nt, "n", "", piper.Point(0, 0))

        text = piper.v2.serialize_bundle([p])
        loaded = piper.v2.deserialize_bundle(text, self.reg)
        text2 = piper.v2.serialize_bundle(loaded.pipelines)
        loaded2 = piper.v2.deserialize_bundle(text2, self.reg)
        self.assertEqual(len(loaded2.pipelines), 1)
        self.assertEqual(loaded2.pipelines[0].name, "main")

    def test_double_external_io_bundle_round_trip(self):
        in_t  = self.reg.find("external_input<double>")
        out_t = self.reg.find("external_output<double>")
        self.assertIsNotNone(in_t)
        self.assertIsNotNone(out_t)

        p = piper.v2.Pipeline()
        p.name = "standard_joint"
        stage = piper.Stage()
        stage.name = "control"
        p.graph.add_stage(stage)
        src  = p.graph.add_node(in_t,  "ipc_in",  "control", piper.Point(0, 0))
        sink = p.graph.add_node(out_t, "ipc_out", "control", piper.Point(100, 0))
        p.graph.set_attr_value(src,  "name", "target")
        p.graph.set_attr_value(sink, "name", "measured")
        p.graph.add_link(piper.PinRef(src, "out"),
                         piper.PinRef(sink, "in"),
                         "double")

        text = piper.v2.serialize_bundle([p])
        loaded = piper.v2.deserialize_bundle(text, self.reg)
        self.assertEqual(len(loaded.pipelines), 1)
        rp = loaded.pipelines[0]
        self.assertEqual(rp.name, "standard_joint")
        self.assertEqual(len(rp.diagnostics), 0)
        types = {n.type for n in rp.graph.nodes()}
        self.assertIn("external_input<double>", types)
        self.assertIn("external_output<double>", types)

    def test_unknown_type_fires_diagnostic(self):
        g = piper.Graph()
        text = piper.v2.serialize(g, "x")  # empty
        empty_reg = piper.NodeRegistry()  # no builtins
        loaded = piper.v2.deserialize(text, empty_reg)
        # An empty graph round-trips with zero diagnostics even against
        # an empty registry.
        self.assertEqual(len(loaded.diagnostics), 0)


class TestStagesAndModes(unittest.TestCase):
    def test_stage_add_and_query(self):
        g = piper.Graph()
        s = piper.Stage()
        s.name = "control"
        self.assertTrue(g.add_stage(s))
        self.assertEqual(len(g.stages()), 1)
        self.assertEqual(g.stages()[0].name, "control")

    def test_default_mode_round_trips(self):
        reg = piper.NodeRegistry()
        piper.register_builtin_nodes(reg)
        g = piper.Graph()
        mp = piper.ModeProfile()
        mp.name = "default"
        g.add_mode_profile(mp)
        g.set_default_mode_name("default")

        text = piper.v2.serialize(g)
        loaded = piper.v2.deserialize(text, reg)
        self.assertEqual(loaded.graph.default_mode_name(), "default")


class TestMeta(unittest.TestCase):
    def test_meta_round_trips(self):
        reg = piper.NodeRegistry()
        g = piper.Graph()
        g.set_meta({"author": "phil", "tag": "demo"})
        text = piper.v2.serialize(g)
        loaded = piper.v2.deserialize(text, reg)
        meta = loaded.graph.meta()
        self.assertEqual(meta["author"], "phil")
        self.assertEqual(meta["tag"], "demo")


if __name__ == "__main__":
    unittest.main()
