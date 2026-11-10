"""Per-label members and Python step callbacks."""
import unittest

import piper
import piper.engine as eng


class LabelMembersTests(unittest.TestCase):
    def graph(self):
        registry = piper.NodeRegistry()
        piper.register_builtin_nodes(registry)
        graph = piper.Graph()
        stage = piper.Stage()
        stage.name = "control"
        graph.add_stage(stage)
        node = graph.add_node(registry.find("constant<float>"), "source", "control", piper.Point(0, 0))
        graph.set_attr_value(node, "value", "5")
        for profile, label in (("slow", "ten"), ("fast", "hundred"), ("same", "hundred"), ("off", "disable")):
            mode = piper.ModeProfile()
            mode.name = profile
            mode.per_node = {node: label}
            graph.add_mode_profile(mode)
        graph.set_default_mode_name("slow")
        self.assertTrue(graph.set_members_per_label(node, True))
        self.assertTrue(graph.set_label_member(node, "ten", "value", "10"))
        self.assertTrue(graph.set_label_member(node, "hundred", "value", "100"))
        return graph, registry, node

    def test_configuration_and_roundtrip(self):
        graph, registry, node = self.graph()
        self.assertTrue(graph.set_node_mode_label("slow", node, "custom"))
        self.assertTrue(graph.set_label_member(node, "custom", "value", "100"))
        self.assertFalse(graph.set_label_member(node, "custom", "not_a_member", "1"))
        loaded = piper.v2.deserialize(piper.v2.serialize(graph), registry)
        self.assertEqual(loaded.diagnostics, [])
        source = next(n for n in loaded.graph.nodes() if n.id == node)
        self.assertTrue(source.members_per_label)
        self.assertEqual(source.members_for_label("custom"), {"value": "100"})
        with self.assertRaises(IndexError):
            source.members_for_label("unknown")
        self.assertEqual(source.members_for_label("disable"), {"value": "5"})
        self.assertTrue(graph.set_members_per_label(node, False, "hundred"))
        source = next(n for n in graph.nodes() if n.id == node)
        self.assertFalse(source.members_per_label)
        self.assertEqual(source.shared_members(), {"value": "100"})
        self.assertEqual(source.label_members, {})

    def test_python_callback_sees_installed_label_and_members(self):
        class Source(eng.Step):
            def declare_io(self):
                self.events = [("declare", self.current_label(), self.read_member("value"))]
                self.declare_output_float("out")
                self.ticks = 0

            def on_label_changed(self):
                self.events.append(("label", self.current_label(), self.read_member("value")))
                self.cached = float(self.read_member("value"))

            def compute(self, stage):
                self.ticks += 1
                self.set_output_float("out", self.cached)

        graph, _, node = self.graph()
        registry = eng.StepRegistry()
        eng.register_step_type_py(registry, "constant<float>", Source)
        engine = eng.Engine()
        result = engine.build(graph, registry)
        self.assertTrue(result.ok, [d.message for d in result.diagnostics])
        step = engine.step_for(node)
        self.assertEqual(step.events, [("declare", "ten", "10"), ("label", "ten", "10")])
        engine.play()
        engine.set_mode("fast")
        self.assertEqual(step.current_mode(), "fast")
        engine.play()
        self.assertEqual(step.read_output_float("out"), 100)
        engine.set_mode("same")
        self.assertEqual(len(step.events), 3)
        engine.set_mode("off")
        self.assertEqual(step.events[-1], ("label", "disable", "5"))
        engine.play()
        self.assertEqual(step.ticks, 2)

    def test_all_labels_exist_before_any_edit(self):
        graph, _, node = self.graph()
        self.assertTrue(graph.ensure_member_labels(node, ["neutral"]))
        source = next(n for n in graph.nodes() if n.id == node)
        for label in ("", "enable", "disable", "neutral", "ten", "hundred"):
            self.assertIn(label, source.label_members)
        graph.set_label_member(node, "neutral", "value", "25")
        graph.set_attr_value(node, "value", "999")
        source = next(n for n in graph.nodes() if n.id == node)
        self.assertEqual(source.members_for_label("neutral"), {"value": "25"})
        self.assertEqual(source.members_for_label("enable"), {"value": "5"})
        self.assertEqual(source.members_for_label("disable"), {"value": "5"})
        self.assertTrue(graph.set_node_mode_label("slow", node, "new"))
        source = next(n for n in graph.nodes() if n.id == node)
        self.assertEqual(source.members_for_label("new"), {"value": "999"})

    def test_builtin_constant_tracks_label(self):
        graph, _, node = self.graph()
        registry = eng.StepRegistry()
        eng.register_builtin_steps(registry)
        engine = eng.Engine()
        self.assertTrue(engine.build(graph, registry).ok)
        self.assertEqual(engine.step_for(node).read_output_float("out"), 10)
        engine.set_mode("fast")
        self.assertEqual(engine.step_for(node).read_output_float("out"), 100)


if __name__ == "__main__":
    unittest.main()
