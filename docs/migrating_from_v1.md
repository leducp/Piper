# Migrating from V1 JSON

The `piper-migrate` CLI converts V1 Piper JSON exports into the
current `.piper` bundle format. This is a one-shot conversion --
once a graph is in V2, it is edited and saved through the new editor.

The CLI is self-documented (`piper-migrate --help`) and the flag
reference lives in [`migrate/README.md`](../migrate/README.md). This
page covers the conceptual mapping and the workflow.

## Workflow

```
$ piper-migrate legacy_pipeline.json -o pipeline.piper
warning [link-orphaned-node] (control_loop): link from 'src.out' to 'ghost.in' ...
piper-migrate: wrote pipeline.piper (1 pipeline(s))
$ piper-editor pipeline.piper
```

Recommended sequence:

1. Run with `--dry-run` first. Inspect the warnings without producing
   output -- this lets you assess whether the V1 file references node
   types the current registry knows about.
2. If the registry is missing types, register them in
   `app/builtin_nodes.cc` (or your downstream registry catalog) and
   re-run.
3. Once warnings are acceptable, run without `--dry-run` to produce
   the `.piper` file.
4. Open the migrated file in the editor and re-arrange node positions
   (V1 did not export positions). Save again to commit the layout.

## Conceptual mapping

| V1                                                  | V2                                                      |
|---                                                  |---                                                      |
| Top-level object keyed by pipeline name             | One entry per pipeline in the bundle's `pipelines[]`    |
| Pipeline name (the outer key)                       | `Pipeline.name`                                         |
| `Stages` (array of strings)                         | Stage entries with default opaque-white color           |
| `Nodes` keyed by node name                          | Numeric `Node.id` + `Node.name` = the V1 name           |
| Flat `node[attr] = value` pairs                     | `Attribute.value` (as string), retyped at write time    |
| `Links` array (`{from, out, to, in, type}`)         | Numeric `Link.id` + `PinRef` endpoints                  |
| `Modes.default`                                     | `Pipeline.default_mode`                                 |
| Per-profile `configuration` map (node names)        | `ModeProfile.per_node` keyed by node id                 |
| `"Enable"` / `"Disable"` / `"Neutral"`              | `"enable"` / `"disable"` / `"neutral"` (lowercased)     |

`"neutral"` is preserved verbatim. V2 does not assign it any built-in
visual; the consuming engine decides what it means.

## What does NOT carry across

- **Node positions** -- V1 stored layout in a separate scene file the
  migrator does not read. Nodes land at `(0, 0)` after migration.
- **Per-pin stage tagging** -- A V2 feature with no V1 equivalent.
  All pins inherit `Node.stage` until the user adds explicit tags in
  the editor.
- **Custom mode-profile colors** -- V2's theme-driven mode coloring
  is opt-in after migration.

## Critical vs. recoverable diagnostics

The reader splits warnings into two tiers:

- **Critical** -- the V1 file references something the migrator
  cannot reproduce in V2 without losing data. Currently this is
  `UnknownNodeType`. The CLI exits non-zero and writes nothing.
  Resolve by registering the type in your registry and re-running.
- **Recoverable** -- e.g. an orphan link (referencing a node that
  does not exist in the V1 file). The link is dropped, a warning is
  printed, and the rest of the conversion proceeds.

`--strict` upgrades recoverable diagnostics into errors. Use it in CI
when you want byte-stable migration output.

## Editor compatibility

The editor accepts both the new wrapped bundle shape
(`{"version": 2, "pipelines": [...]}`) and the legacy unwrapped V2
shape produced by earlier alpha builds. Files are always *written*
in the wrapped shape going forward.
