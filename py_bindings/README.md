# piper Python bindings

A nanobind-built CPython module exposing Piper's domain layer (graph,
node registry, V2 serializer) as a normal Python import. **No editor,
no GUI** -- this is for scripted construction, inspection, and
batch-conversion of `.piper` files.

```python
import piper

reg = piper.NodeRegistry()
piper.register_builtin_nodes(reg)

g = piper.Graph()
src  = g.add_node(reg.find("sin_wave"),     "src",  "control", piper.Point(0, 0))
sink = g.add_node(reg.find("probe<float>"), "sink", "control", piper.Point(120, 0))
g.add_link(piper.PinRef(src, "out"),
           piper.PinRef(sink, "in"),
           "float")

with open("demo.piper", "w") as f:
    f.write(piper.v2.serialize(g, "main"))
```

## Install

The wheel is built with [scikit-build-core](https://scikit-build-core.readthedocs.io)
on top of the C++ CMake build. From a fresh checkout:

```
# uv (recommended)
uv pip install .

# or pip from a regular venv
pip install .
```

The build pulls deps from Conan (matching the editor's setup) and
compiles `piper_core` + the nanobind module. Only Python >= 3.10.

## What's exposed

### Top-level

| Symbol                          | Notes |
|---                              |---    |
| `piper.NodeRegistry`            | `add(node_type)`, `find(name) -> NodeType`, `all() -> list[NodeType]`. |
| `piper.register_builtin_nodes(registry)` | Adds Piper's bundled types (`sin_wave`, `add`, `low_pass`, ...). |
| `piper.NodeType`                | Mutable; fields: `type`, `library`, `category`, `help`, `attributes`. |
| `piper.AttributeSpec`           | Fields: `name`, `data_type`, `role` (= `AttributeSpec.Role.{Input,Output,Member}`), `default_value`. |
| `piper.Graph`                   | The graph mutators (`add_node`, `add_link`, `set_attr_value`, ...) and accessors (`nodes()`, `links()`, ...). |
| `piper.Node`, `piper.Link`      | Returned by graph accessors. `Link.from_` (Python keyword `from` is reserved). |
| `piper.PinRef`                  | `(node, attr)` pair used in link endpoints. |
| `piper.Attribute`               | Per-instance attribute snapshot (with `value` as string). |
| `piper.Stage`, `piper.ModeProfile` | Graph-level metadata. |
| `piper.Point(x, y)`             | 2D position used by `Node.pos`. |
| `piper.Diagnostic`, `piper.DiagnosticKind` | Loader diagnostics. |
| `piper.invalid_node_id`, `piper.invalid_link_id` | Sentinel zeros. |

### `piper.v2` submodule

| Symbol                              | Notes |
|---                                  |---    |
| `v2.serialize(graph, name="")`      | Single-pipeline file. Returns JSON string. |
| `v2.serialize_bundle(pipelines)`    | Multi-pipeline file. Argument: a list of `v2.Pipeline` objects. |
| `v2.deserialize(text, registry)`    | Returns `LoadResult { graph, diagnostics }` -- first pipeline only. |
| `v2.deserialize_bundle(text, registry)` | Returns `BundleLoadResult { pipelines: list[Pipeline], diagnostics }`. |
| `v2.Pipeline`                       | Fields: `name`, `graph`, `diagnostics`. |
| `v2.LoadResult`, `v2.BundleLoadResult` | Loader return types. |

## Examples

### Programmatic graph construction

```python
import piper

reg = piper.NodeRegistry()
piper.register_builtin_nodes(reg)

g = piper.Graph()
g.add_stage(_stage("control"))

const_t = reg.find("constant<float>")
add_t   = reg.find("add")
probe_t = reg.find("probe<float>")

a = g.add_node(const_t, "a", "control", piper.Point(0,   0))
b = g.add_node(const_t, "b", "control", piper.Point(0, 100))
sum_node = g.add_node(add_t,   "sum",  "control", piper.Point(180, 50))
out_node = g.add_node(probe_t, "out",  "control", piper.Point(360, 50))

g.set_attr_value(a, "value", "1.0")
g.set_attr_value(b, "value", "2.0")

g.add_link(piper.PinRef(a, "out"), piper.PinRef(sum_node, "a"), "float")
g.add_link(piper.PinRef(b, "out"), piper.PinRef(sum_node, "b"), "float")
g.add_link(piper.PinRef(sum_node, "out"), piper.PinRef(out_node, "in"), "float")

print(piper.v2.serialize(g, "summed"))


def _stage(name):
    s = piper.Stage()
    s.name = name
    return s
```

### Inspecting an existing `.piper` file

```python
import piper

reg = piper.NodeRegistry()
piper.register_builtin_nodes(reg)

with open("examples/filter_demo/filter_demo.piper") as f:
    bundle = piper.v2.deserialize_bundle(f.read(), reg)

for p in bundle.pipelines:
    print(f"== {p.name or '(unnamed)'}: "
          f"{len(p.graph.nodes())} nodes, {len(p.graph.links())} links")
    for n in p.graph.nodes():
        print(f"  - {n.name} ({n.type}) @ {n.pos.x},{n.pos.y}")
    for d in p.diagnostics:
        print(f"  ! {d.message}")
```

### Batch-validating a directory of files

```python
import piper, pathlib

reg = piper.NodeRegistry()
piper.register_builtin_nodes(reg)

for path in pathlib.Path("pipelines").glob("*.piper"):
    bundle = piper.v2.deserialize_bundle(path.read_text(), reg)
    bad = sum(len(p.diagnostics) for p in bundle.pipelines) + len(bundle.diagnostics)
    print(f"{path.name}: {bad} diagnostic(s)")
```

## Tests

The bundled tests live in `py_bindings/tests/`. Run them via the
project's CMake / ctest harness:

```
cmake -S . -B build -DBUILD_PY_BINDINGS=ON
cmake --build build
ctest --test-dir build -R piper_py
```

Or directly with `unittest` from a built wheel:

```
PYTHONPATH=build/py_bindings python -m unittest \
    discover -s py_bindings/tests -p "test_*.py"
```
