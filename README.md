# Piper

Visual designer for control-system pipelines. Piper produces JSON
that an external engine consumes -- the editor is designer-only and
never runs the graph itself. Built on ImGui + GLFW + Conan 2.

```
target_x ----+
              +-- jacobian -- motor_a -- pose_a    [control / feedback]
target_y ----+               motor_b -- pose_b
```

## Status

The editor is usable end-to-end: build, save, load, edit, undo,
paste, multi-stage, multi-mode, multi-document. The `piper-migrate`
CLI imports V1 JSON. Python bindings expose the domain layer for
scripting and batch validation.

The V1 sources live on the `v1_legacy` branch for anyone who needs
them. See [`docs/migrating_from_v1.md`](docs/migrating_from_v1.md)
for the conversion workflow.

## Quick start

```
git clone <repo>
cd Piper
./setup_build.sh build           # generates Conan profile + installs deps
cmake -S . -B build
cmake --build build -j

./build/app/piper-editor examples/engine_demo.piper
```

The editor opens to a low-pass filter demo (`examples/engine_demo/README.md`).
For a multi-stage motor-control walkthrough see
`docs/motor_control_walkthrough.md`.

## Concepts in 60 seconds

- **Node** -- a unit of computation with named pins and member
  attributes. Type comes from a registry (`SinWave`, `low_pass`,
  `motor`, ...).
- **Pin** -- an Input or Output attribute on a node. Pins are typed
  (`float`, `int`, `vec3`, ...) and connect via Links.
- **Link** -- a typed edge from an output pin to one or more input
  pins. Fan-out and fan-in are both allowed; runtime semantics are
  the engine's call.
- **Stage** -- which nodes are *running* in this slice of the
  pipeline (`control`, `feedback`, ...). The engine topologically
  sorts each stage's subgraph; cycles close across stages.
- **Mode profile** -- a *meta-mode* that picks a label per node
  (`enable`, `disable`, custom). Switching profiles is how a user
  reconfigures behaviour without rewiring.

Architecture details: see `docs/architecture.md`.

## Repository layout

```
canvas/         reusable ImGui node-editor framework
core/           domain layer: graph data, JSON (de)serializer, command stack
app/            piper-editor binary (uses canvas + core)
migrate/        V1 JSON import CLI
py_bindings/    nanobind wheel exposing the domain layer to Python
examples/       bundled .piper graphs
data/           default theme.json
docs/           architecture, format, walkthroughs
tests/          gtest suites for core, canvas, fixtures
```

Per-subdir READMEs:

- [`canvas/README.md`](canvas/README.md) -- the framework's API
  surface, layering rule (no piper_core dep), pull/push contract,
  events, hooks.
- [`core/README.md`](core/README.md) -- domain types, file-format
  pointer, how to register a new node type, how to drive it
  programmatically.
- [`app/README.md`](app/README.md) -- running the editor, layout,
  mouse + keyboard reference, theme, architecture sketch.
- [`migrate/README.md`](migrate/README.md) -- `piper-migrate` CLI:
  options, exit codes, what does and doesn't translate.
- [`py_bindings/README.md`](py_bindings/README.md) -- Python module:
  install, exposed API, examples.

## Reference docs

- [`docs/architecture.md`](docs/architecture.md) -- canvas / core /
  app layering, pull-then-push render model.
- [`docs/v2_format.md`](docs/v2_format.md) -- on-disk JSON schema
  for graphs and registries (the file's `version` field is `2`).
- [`docs/type_system.md`](docs/type_system.md) -- type tags, the
  pastel hue-index helper, the default palette.
- [`docs/motor_control_walkthrough.md`](docs/motor_control_walkthrough.md)
  -- end-user walkthrough using the bundled examples.

## Building

Requires CMake 3.28, Conan 2.10+, a C++20 toolchain, and either GLFW
system deps (Linux: `libxkbcommon-dev`, `libwayland-dev`, ...) or
the equivalent on macOS / Windows. `setup_build.sh` generates a
Conan profile and resolves dependencies.

## Tests

```
cmake --build build
ctest --test-dir build
```

Two gtest binaries:

- `build/tests/core/piper_core_test` -- 153 cases over the data
  model, JSON round-trip, theme parsing, command stack, lints.
- `build/tests/canvas/piper_canvas_test` -- 53 cases over the
  framework's math (AABB, transform, hit-test, link routing, pin
  layout, selection).

The canvas demo (`build/examples/canvas_demo/canvas_demo`) drives
the framework over a tiny `DemoGraph` independent of `piper_core`
-- it's the framework's smoke test in human form.

## Licence

CeCILL-C.
