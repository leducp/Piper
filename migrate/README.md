# piper-migrate

CLI that converts V1 Piper JSON to the current `.piper` file format. Produces a V2 bundle (one pipeline per V1 top-level
key); pipes the result through the same serializer the editor uses.

```
piper-migrate <input.json> [-o <output.piper>] [--dry-run] [--strict]
```

## Options

| Flag         | Meaning |
|---           |---      |
| `-o, --output PATH` | Output file. Defaults to the input path with the extension swapped to `.piper`. |
| `--dry-run`  | Parse and report; do not write the output file. |
| `--strict`   | Treat any reader warning as an error. Exit non-zero, no output. |

## Exit codes

| Code | Meaning |
|---   |---      |
| `0`  | Success. Output written (unless `--dry-run`). |
| `1`  | Critical diagnostic (currently: any unknown node type) -- the V1 file references a node type the registry does not know, so a partial conversion would lose data. No output written. |
| `2`  | `--strict` was set and at least one diagnostic fired. No output written. |

## What gets translated

- Top-level pipeline names (V1's outer object keys) become entries in
  the V2 bundle's `pipelines[]`.
- `Stages` (V1: array of strings) -> V2 stage entries (default opaque
  white color).
- `Nodes` (V1: object keyed by node name) -> V2 nodes with stable
  numeric ids; the V1 name becomes `node.name`.
- Per-node attribute key/value pairs are mapped onto the registry's
  declared `AttributeSpec` entries; values are preserved verbatim as
  strings (the V2 serializer will retype numeric / boolean values
  based on `data_type` at write time).
- `Links` (V1: array of `{from, out, to, in, type}`) -> V2 links with
  fresh numeric ids; `data_type` is preserved.
- `Modes` -> V2 mode profiles. `Modes.default` (V1's reserved key)
  becomes `default_mode`. Each profile's `configuration` map is
  remapped from node names to node ids; labels are lowercased
  (`"Enable"` -> `"enable"`, `"Disable"` -> `"disable"`,
  `"Neutral"` -> `"neutral"`).

## What is NOT translated

- **Per-pin stage tags** (a V2 feature). V1 has no equivalent; the
  migrator emits empty `stages` lists on every attribute, meaning all
  pins inherit `node.stage`. Users can opt into per-pin tagging in V2
  after migration.
- **Node positions**. V1's positions are not in the JSON export
  (they're in a separate `.piper` "scene" file format which V2 does
  not consume). Migrated nodes land at `(0, 0)`; reposition them in
  the editor.
- **Custom mode-profile colors** (introduced in V2). V1 had no theme
  layer for modes; defaults apply.

## Diagnostics

The reader emits the following kinds (printed to stderr, prefixed by
the originating pipeline name):

| Kind                       | Action |
|---                         |---     |
| `unknown-node-type`        | **Critical**: aborts the run. The registry does not declare the type. |
| `attribute-missing`        | The V1 node carries an attribute name not in the registry's spec; the value is dropped. |
| `link-orphaned-node`       | The link references a node name that does not exist; the link is dropped. |
| `link-orphaned-attribute`  | The link references a pin name not on the resolved node; the link is dropped. |
| `orphan-mode-reference`    | A mode profile's `configuration` references a node name that does not exist. |
| `duplicate-stage-name`     | Two `Stages` entries share a name. |
| `duplicate-profile-name`   | Two profiles share a key. |
| `schema-error`             | Other malformed entries (non-object node, non-string label, etc.). |

`--strict` upgrades all non-critical diagnostics into hard errors.

## Examples

```
# Migrate a single-pipeline V1 file.
piper-migrate legacy.json -o new.piper

# Inspect the conversion without writing.
piper-migrate legacy.json --dry-run

# Refuse to convert if anything is suspicious.
piper-migrate legacy.json -o new.piper --strict
```

## Build

`piper-migrate` is built by the top-level CMake. It links only
`piper_core` + `argparse` + `nlohmann_json` (no ImGui, no GLFW).
