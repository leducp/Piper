# Piper V2 architecture

> Skeleton — populated incrementally as each epic lands. The
> authoritative living version of this document lives alongside the
> code; the original design plan is preserved in
> `.claude/plans/fuzzy-mixing-diffie.md`.

## Components

V2 is split into peer subdirectories with a strict layering rule:

```
core/      (piper)              domain layer, no GUI deps
canvas/    (piper::canvas)      reusable node-editor framework, no Piper deps
app/       (piper-editor)       links core + canvas + ImGui + GLFW
migrate/   (piper-migrate)      links core only
py_bindings/                    links core only via nanobind
```

`canvas/` MUST NOT link `core/`. `migrate/` and `py_bindings/` MUST
NOT link `canvas/` or any GUI dependency. Enforced in CMake.

## Validation boundary

V2 is a designer; it does NOT execute the pipeline. It owns
*structural* validity (link type compatibility, schema correctness)
and the engine consuming the V2 JSON file owns *semantic* validity
(execution order, cycles, units, sample rates).

Full validation table: see the design plan, "Validation boundary"
section. Reproduced as part of `docs/v2_format.md` once that lands.

## Data flow at runtime

```
   ┌────────┐ user input ┌─────────┐ events  ┌─────────────────┐
   │ window │───────────▶│ canvas  │────────▶│ host (app)      │
   │ GLFW   │            │ Editor  │         │ MainWindow      │
   └────────┘◀───────────│         │◀────────│ + CommandStack  │
              draw cmds  └─────────┘  spans  │ + piper::Graph  │
                                             └─────────────────┘
```

Pull for state, push for changes. Detail: design plan, "Framework
API surface" section.

## Sub-PR roadmap

This document fills out as the implementation lands. Track which
sub-PRs are merged on the master branch.
