# Architecture

## Components

Piper is a flat layout of peer subdirectories with a strict layering
rule. Each component owns one concern and the dependency graph is
acyclic.

```
core/        (piper)         domain layer: graph data, registry, V2 (de)serializer,
                             command stack, diagnostics, theme parsing.
                             Depends on: stdlib + nlohmann_json. No GUI deps.

canvas/      (piper::canvas) reusable ImGui node-editor framework.
                             Depends on: imgui only. Domain-agnostic.
                             MUST NOT link piper_core.

app/         (piper-editor)  GUI application. Depends on:
                             piper_core + piper_canvas + ImGui + GLFW
                             + portable-file-dialogs.

migrate/     (piper-migrate) V1 -> V2 CLI. Depends on:
                             piper_core + argparse + nlohmann_json.

py_bindings/ (piper python)  nanobind module. Depends on: piper_core only.
                             No GUI, no ImGui.
```

The CMake top level enforces this: only `app/` declares ImGui or GLFW
dependencies, only `app/` and `examples/canvas_demo/` link
`piper_canvas`, and `py_bindings/` and `migrate/` cannot reach into
GUI dependencies even by accident.

## Pull-then-push rendering

The canvas framework reads graph state and emits intent events; the
host application owns mutation. This decouples the framework from any
specific domain model.

```
   ┌────────┐ user input ┌─────────┐ events  ┌─────────────────┐
   │ window │───────────>│ canvas  │────────>│ host (app)      │
   │ GLFW   │            │ Editor  │         │ MainWindow      │
   └────────┘<───────────│         │<────────│ + CommandStack  │
              draw cmds  └─────────┘  spans  │ + piper::Graph  │
                                             └─────────────────┘
```

Each frame:

1. **Pull**: the host's adapter (`PiperCanvasGraph`) translates its
   `piper::Graph` mirror into `canvas::Node` / `canvas::Link` spans
   the framework draws.
2. **Render**: `canvas::Editor::draw()` produces ImGui draw commands
   for the current viewport; widgets win clicks before
   `InvisibleButton`s collect drags (PR 2.10's render-then-input
   pattern).
3. **Events**: the host calls `Editor::consume_events()` and turns
   each event into a `piper::Command` pushed onto the document's
   `CommandStack`. The next frame's `pull` reflects the change.

The framework holds zero domain state. The adapter is the only piece
that knows about both worlds.

## CommandStack and undo

All graph mutations -- whether triggered by the canvas, the
inspector, the stages panel, the modes panel, the right-click menu,
or paste -- flow through `CommandStack::push(cmd, graph)`. Each
command captures enough state to apply and revert deterministically.

Drag interactions and paste fan out into multiple primitive commands;
they coalesce into one undo entry by wrapping the dispatch in
`open_group() / close_group()`. Save clears the document's dirty flag.

## Multi-document host

`MainWindow` owns `vector<unique_ptr<Document>>`. Each `Document`
carries its own `Graph`, `CommandStack`, `canvas::Editor`,
`PiperCanvasGraph` adapter, selection, current stage, active mode
profile, and load/lint diagnostics. A tab bar across the top selects
the active document; only the active document's editor draws each
frame.

Shared state lives on `MainWindow`: the `Theme`, the `NodeRegistry`,
the `mode_color_table`, and the cross-tab clipboard. Document
addresses are stable (held via `unique_ptr`) so the editor's
references into per-doc fields stay valid across tab insert/erase.

Save semantics preserve multi-pipeline files: hitting Ctrl+S on one
tab gathers every other tab whose `loaded_path` matches and writes
them as one bundle. This means a file that came in as a multi-
pipeline bundle stays multi-pipeline after a single-tab edit.

## File format

V2 wraps any number of pipelines in a top-level `pipelines[]` array.
The full schema lives in [`v2_format.md`](v2_format.md).
[`type_system.md`](type_system.md) covers the type-tag conventions
and the bundled palette.

## Validation boundary

V2 is the *designer*. It owns structural validity:

- Pin type compatibility on link creation (string-tag equality, plus
  same-node and already-connected guards).
- Visual highlight of incompatible pins during drag.
- Re-check link types on graph load (catches drift when a node
  type's attribute changed since the graph was saved).
- Schema validity on save / load.

It does **not** own semantic validity. The engine consuming V2 JSON
owns:

- Within-stage execution order (topological sort).
- Cycle detection / causality / fixed-point convergence.
- Stage-dependent pin direction resolution (the "bus" pattern).
- Numerical type promotion (e.g. `int -> float`).
- Sample-rate / unit consistency.

V2 does not enforce engine-level rules -- doing so would reject valid
graphs. The engine does not enforce structural rules -- doing so
would duplicate the editor.

## Engine execution model

`Engine::play()` runs each stage once per tick, in the order they
appear in `graph.stages()`. Within a stage, the engine builds a
Kahn topological sort over the links between *same-stage* nodes
(`engine.cc`, "Per-stage topo sort"). Producer steps always run
before same-stage consumers, so a consumer reads the current
tick's value. A cycle inside one stage is a build error
(`CycleDetected`).

Across stages, what a consumer sees depends on tick order:

- **Producer's stage runs first**: the consumer reads the
  producer's *current-tick* output.
- **Producer's stage runs after**: the consumer reads the
  producer's *previous-tick* output. This single-tick break
  is how the engine resolves feedback loops -- a cycle that
  would be illegal inside one stage becomes a legal recurrence
  once it crosses a stage boundary. The motor `command`
  (`control`) / `measured` (`feedback`) Bus pattern in
  `motor_control_dual_jacobian.piper` is the canonical case.

Open-loop DAGs do not need multiple stages. The bundled
`engine_demo.piper` would produce identical samples if its
`generate / control / feedback` stages were collapsed into one.
Add a stage when:

- A loop closes back on itself and you need a 1-tick break to
  cut the cycle.
- A subgraph should run at a different cadence than the rest
  (a host can call `tick(stage)` selectively instead of `play()`).
- You want the editor to visually separate phases of the
  pipeline.

### Modes

A `ModeProfile` is `name + per_node[NodeId] = label`. Each node
carries its own label per profile, and "switching profile" means
"look up each node's label in the new profile". Switching is
done via `Engine::set_mode("name")`; the graph's `default_mode`
is applied automatically at `build()`.

The engine reserves exactly one label, `"disable"` -- nodes
labeled `"disable"` in the active profile are skipped at tick
time (their `compute()` is not called and their outputs hold
their last-written value, zero on a fresh build). Every other
label (including `"enable"` and any host-defined string) is
opaque to the engine; the step reads it via
`Step::current_label()` and decides what to do. So a step that
exposes several "computational slots" can dispatch on its label
inside `compute()` -- e.g. publish a different stored value, run
a different update rule, route a different input downstream --
without the engine knowing anything about the slot semantics.

`Step::current_mode()` returns the active profile handle (handy
when several nodes need to coordinate on a single broadcast
value); `Step::current_label()` returns *this* node's per-profile
label handle. A step typically uses the label. Both return a
`Mode` -- a `{name, id}` struct where `id` is the FNV-1a hash
pre-computed once on `set_mode()`. Comparison with a string
literal folds the literal hash at -O1+, so the hot path is a
single uint64 compare with no string work:

```cpp
void compute(Stage) override
{
    if (current_label() == "loose")
    {
        ...
    }
}
```

Same trick as `Stage`, sharing `hash_name()` under the hood.

Setting a mode name not in `graph.mode_profiles` is allowed:
`current_mode()` reports it verbatim, every node's
`current_label()` is empty, nothing is gated. Lets the host
expose modes meaningful to step code without listing them in the
file.

## Extending Piper

- **Adding a node type**: declare it in `core/src/builtin_nodes.cc`
  (or a downstream registry). Walkthrough:
  [`adding_a_node_type.md`](type_system.md#registering-a-node-type).
- **Adding a command**: subclass `piper::Command`, implement `apply`
  and `revert`, expose a constructor. The pattern is consistent
  across the existing commands in `core/include/piper/commands.h`.
- **Adding a diagnostic**: extend `DiagnosticKind` in
  `core/include/piper/diagnostic.h`; the loader and the editor's
  Problems panel pick it up automatically.
- **Replacing the canvas framework**: `canvas/` is reusable -- swap
  the adapter and the host can keep its `Graph` model unchanged.
