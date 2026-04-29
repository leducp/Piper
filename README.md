# Piper

Visual designer for control-system pipelines. Piper produces V2 JSON
that an external engine consumes -- the editor is designer-only and
never runs the graph itself. Originally a Qt5 node editor; V2 has
been ported to ImGui + GLFW + Conan 2 to share infrastructure with
the sibling real-time-monitor project.

```
target_x ----+
              +-- jacobian -- motor_a -- pose_a    [control / feedback]
target_y ----+               motor_b -- pose_b
```

## Status

V2 is in alpha: the editor is usable end-to-end (build, save, load,
edit, undo, paste, multi-stage, multi-mode). What's not done:

- Migration tool from V1 JSON (Epic 3 -- next).
- Python bindings (Epic 5).
- A few editor polish items (full SetModeProfileCommand undo, native
  splitter cursor on Windows, ...).

V1 is preserved on the `v1_legacy` branch. Anyone still using
Qt5-Piper checks out that branch; everyone else builds master.

## Quick start

```
git clone <repo>
cd Piper
./setup_build.sh build           # generates Conan profile + installs deps
cmake -S . -B build
cmake --build build -j

./build/app/piper-editor examples/motor_control_simple.piper
```

The editor opens to a single-channel motor-control example. Walk
through it with `docs/motor_control_walkthrough.md`.

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

The plan in detail: see `docs/architecture.md`.

## Repository layout

```
canvas/         reusable ImGui node-editor framework
core/           domain layer: graph data, V2 (de)serializer, command stack
app/            piper-editor binary (uses canvas + core)
migrate/        V1 -> V2 CLI (planned, Epic 3)
py_bindings/    nanobind wheel (planned, Epic 5)
examples/       bundled .piper graphs
data/           default theme.json
docs/           architecture, format, walkthroughs
tests/          gtest suites for core, canvas, fixtures
```

Per-subdir READMEs:

- [`canvas/README.md`](canvas/README.md) -- the framework's API
  surface, layering rule (no piper_core dep), pull/push contract,
  events, hooks.
- [`core/README.md`](core/README.md) -- domain types, V2 file
  format pointer, how to register a new node type, how to drive
  it programmatically.
- [`app/README.md`](app/README.md) -- running the editor, layout,
  mouse + keyboard reference, theme, architecture sketch.

## Reference docs

- [`docs/architecture.md`](docs/architecture.md) -- canvas / core /
  app layering, pull-then-push render model.
- [`docs/v2_format.md`](docs/v2_format.md) -- V2 JSON schema for
  graphs and registries.
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

- `build/tests/core/piper_core_test` -- 147 cases over the data
  model, V2 round-trip, theme parsing, command stack, lints.
- `build/tests/canvas/piper_canvas_test` -- 53 cases over the
  framework's math (AABB, transform, hit-test, link routing, pin
  layout, selection).

The canvas demo (`build/examples/canvas_demo/canvas_demo`) drives
the framework over a tiny `DemoGraph` independent of `piper_core`
-- it's the framework's smoke test in human form.

## Licence

CeCILL-C (V1 inheritance). Same as before.
