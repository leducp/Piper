# Piper V2 file format

Piper V2 emits two kinds of JSON files: **graph files** describing
designed pipelines (the user's `.piper` documents) and **registry
catalog files** describing the catalog of available node types
(typically engine-supplied). Both share the V2 conventions and the
same `version` integer; their top-level shapes differ.

The format is designed to be consumed by external engines (the
runtime that interprets Piper-designed pipelines) -- not just
round-tripped by Piper itself. This document is the authoritative
schema reference.

## Compatibility policy

V2 has a single integer version number. The compatibility rules for
the V2 lifetime are:

- **Adding a new optional field with a documented default = same V2.**
  Old readers ignore unknown fields; new readers see the default
  when the field is absent. No version bump.
- **Adding a new `DiagnosticKind` value = same V2.** Consumers must
  not enumerate kinds exhaustively; unknown kinds should be
  surfaced as opaque messages.
- **Adding a new builtin node type or stock data type = same V2.**
  Type tags are strings; the format does not enumerate them.
- **Adding a *required* field, removing a field, or changing the
  semantics of an existing field = bump to V3.**
- **Mid-version `version` mismatches throw.** The deserializer
  rejects `version != 2` rather than attempting forward-compatible
  loads.

## Conventions

- Encoding: UTF-8.
- Top-level value: a JSON object.
- Numeric IDs (`NodeId`, `LinkId`) are unsigned 64-bit integers. Zero
  is reserved as "invalid" -- never appears as a real ID.
- Floats are standard JSON numbers. `pos` arrays carry IEEE-754
  values; precision below ~6 significant digits is not guaranteed to
  round-trip identically.
- Color values are hex strings: `"#RRGGBBAA"` (red high byte, alpha
  low byte). Alpha is mandatory -- the 6-digit `#RRGGBB` form is
  rejected. Case-insensitive on read; emitted upper-case.
- Optional fields are omitted when empty/default -- readers must accept
  both absent and explicit-default values.

## Top-level structure

```json
{
    "version": 2,
    "nodes":   [ ... ],
    "links":   [ ... ],
    "stages":  [ ... ],
    "modes":   [ ... ]
}
```

| Field      | Type    | Required | Notes |
|---         |---      |---       |---    |
| `version`  | int     | yes      | Must be `2`. Any other value causes the loader to throw. |
| `nodes`    | array   | optional | Defaults to `[]`. |
| `links`    | array   | optional | Defaults to `[]`. |
| `stages`   | array   | optional | Defaults to `[]`. |
| `modes`    | array   | optional | Defaults to `[]`. |

The deserializer throws `std::runtime_error` only on (a) malformed
JSON or (b) `version != 2`. All other structural problems are
reported as diagnostics; the graph still loads with verbatim data.

## Node

```json
{
    "id":    42,
    "type":  "Bus",
    "name":  "main_bus",
    "stage": "control",
    "pos":   [320, 180],
    "attrs": [ ... ]
}
```

| Field   | Type           | Required | Notes |
|---      |---             |---       |---    |
| `id`    | uint64         | yes      | Stable handle; non-zero, unique within `nodes`. |
| `type`  | string         | yes      | Looked up in the registry. Unknown type -> `UnknownNodeType` diagnostic; node still loads. |
| `name`  | string         | optional | Defaults to `""`. User-editable; not a stable handle. |
| `stage` | string         | optional | Defaults to `""`. References a `stages[].name`; unknown reference -> `UnknownStageReference` diagnostic. |
| `pos`   | [float, float] | optional | Defaults to `[0, 0]`. Canvas coordinates. |
| `attrs` | array          | optional | Defaults to `[]`. See **Attribute** below. |

## Attribute

Per-instance attribute on a node. The `name`/`data_type`/`role`
fields **deliberately duplicate** the registry's `AttributeSpec` -- the
duplication is the load-time drift signal.

```json
{
    "name":      "torque_cmd",
    "data_type": "vec3",
    "role":      "output",
    "value":     "0.0",
    "stages":    ["control"]
}
```

| Field       | Type     | Required | Notes |
|---          |---       |---       |---    |
| `name`      | string   | yes      | Stable handle for `PinRef::attr`. |
| `data_type` | string   | yes      | Type-tag string. Drift versus current registry -> `AttributeDrift` diagnostic. |
| `role`      | string   | yes      | One of `"input"`, `"output"`, `"member"`. Other values -> `SchemaError`. |
| `value`     | string   | optional | Member values (PID gains, default sample rate, etc.). Omitted when empty. |
| `stages`    | string[] | optional | Per-pin stage override. Empty list means "inherit `node.stage`". Omitted when empty. |

If a saved attribute is no longer present in the current registry's
spec for `node.type`, an `AttributeMissing` diagnostic is emitted; the
attribute is still loaded verbatim.

## Link

```json
{
    "id":        7,
    "from":      { "node": 42, "attr": "torque_cmd" },
    "to":        { "node": 11, "attr": "in" },
    "data_type": "vec3"
}
```

| Field       | Type    | Required | Notes |
|---          |---      |---       |---    |
| `id`        | uint64  | yes      | Stable handle; non-zero, unique within `links`. |
| `from`      | object  | yes      | Output endpoint. `{node: NodeId, attr: string}`. |
| `to`        | object  | yes      | Input endpoint. Same shape. |
| `data_type` | string  | optional | Snapshot of the link's data type at creation. Empty allowed but discouraged. |

Diagnostics:

- `LinkOrphanedNode`: either endpoint references a non-existent node.
- `LinkOrphanedAttribute`: endpoint's `attr` is not present on the
  resolved node.
- `LinkTypeMismatch`: `data_type` differs from either endpoint's
  current `data_type`, or endpoints disagree.

The link is always inserted into the graph regardless of which
diagnostic fires (subject to endpoint resolution). The editor surfaces
warnings; the user can re-route or delete.

## Stage

```json
{
    "name":  "control",
    "color": "#FF0000FF"
}
```

| Field   | Type   | Required | Notes |
|---      |---     |---       |---    |
| `name`  | string | yes      | Unique within `stages`. References from nodes/attrs use this name. |
| `color` | string | optional | `"#RRGGBBAA"`. Defaults to opaque white. |

Stage references in `Node.stage` and `Attribute.stages` use this
`name` as the foreign key. If a referenced stage is not in `stages[]`,
an `UnknownStageReference` diagnostic fires; the verbatim string is
preserved on disk and in memory.

Stages are NOT cascaded on remove -- V2 is permissive: deleted stages
leave dangling references that surface as diagnostics on next load.

## Mode profile

```json
{
    "name":       "default",
    "is_default": true,
    "per_node":   [
        {"node": 42, "label": "enable"},
        {"node": 11, "label": "disable"}
    ]
}
```

| Field        | Type    | Required | Notes |
|---           |---      |---       |---    |
| `name`       | string  | yes      | Unique within `modes`. |
| `is_default` | bool    | optional | Defaults to `false`. At most one profile should be marked default. |
| `per_node`   | array   | optional | Each entry is `{node: NodeId, label: string}`. |

`per_node` is keyed by `NodeId`, NOT by `Node::name`. Renaming a node
does not break mode profile entries.

V2 ships two built-in labels: `"enable"` (default visual: full alpha)
and `"disable"` (default visual: dimmed body). Any other label is
opaque to V2 -- it is preserved verbatim and the host application's
`mode_color_table` resolves it to a color.

If a `per_node` entry references a `node` not in the graph, an
`OrphanModeReference` diagnostic fires; the entry is preserved.

## Diagnostic kinds

The deserializer returns a `LoadResult { Graph, vector<Diagnostic> }`.
Diagnostic kinds:

| Kind                       | Triggered by |
|---                         |---           |
| `SchemaError`              | Malformed entry (missing required field, bad role string, malformed `pos`, malformed color, etc.). The offending entry or field is skipped; a default may apply. |
| `DuplicateNodeId`          | Same node ID appears twice in `nodes[]`. |
| `DuplicateLinkId`          | Same link ID appears twice in `links[]`. |
| `DuplicateStageName`       | Same stage name appears twice in `stages[]`. |
| `DuplicateProfileName`     | Same mode profile name appears twice in `modes[]`. |
| `DuplicateTypeName`        | Same node-type name appears twice in `types[]` (registry format). |
| `UnknownNodeType`          | `node.type` not in the registry passed to `deserialize`. |
| `AttributeMissing`         | Saved attribute name not present in the current registry spec. |
| `AttributeAdded`           | Registry spec has an attribute not present in the saved node (engine should apply the default). |
| `AttributeDrift`           | Saved `attribute.data_type` differs from the current spec's `data_type`. |
| `LinkOrphanedNode`         | Link endpoint references a non-existent node. Link is dropped. |
| `LinkOrphanedAttribute`    | Link endpoint references an attribute not on the node. Link is dropped. |
| `LinkTypeMismatch`         | Link's `data_type` differs from endpoint `data_type`s, or endpoints disagree. **Link is still inserted** -- engine consumers must check diagnostics before trusting any link. |
| `OrphanModeReference`      | Mode profile `per_node` entry references a non-existent node. Entry is preserved verbatim. |
| `UnknownStageReference`    | `Node.stage` or `Attribute.stages` references a stage not in `stages[]`. Reference preserved verbatim. |

Each diagnostic carries a human-readable `message` plus optional
locator fields (`node_id`, `attr_name`, `link_id`) that the editor
uses to focus the offending element on click.

## Registry catalog format

A separate JSON shape -- produced by `v2::serialize_registry`,
consumed by `v2::deserialize_registry`. Engines ship their own
catalog file; Piper loads it at startup to know the available node
types. This is independent of any specific graph file.

```json
{
    "version": 2,
    "types": [
        {
            "type": "PID",
            "library": "control",
            "category": "control",
            "help": "Proportional-integral-derivative controller",
            "attributes": [
                {"name": "setpoint", "data_type": "float", "role": "input"},
                {"name": "measured", "data_type": "float", "role": "input"},
                {"name": "out",      "data_type": "float", "role": "output"},
                {"name": "kp",       "data_type": "float", "role": "member", "default_value": "1.0"}
            ]
        }
    ]
}
```

| Top-level field | Type    | Required | Notes |
|---              |---      |---       |---    |
| `version`       | int     | yes      | Must be `2`. |
| `types`         | array   | optional | Defaults to `[]`. Type entries are sorted by name on emit for deterministic diffs. |

Each type entry:

| Field        | Type    | Required | Notes |
|---           |---      |---       |---    |
| `type`       | string  | yes      | Unique within `types`. Duplicate names -> `DuplicateTypeName` diagnostic; first entry wins. |
| `library`    | string  | optional | Free-form tag for grouping in palettes (e.g. `"math"`, `"control"`). |
| `category`   | string  | optional | Free-form tag (e.g. `"filter"`, `"generator"`). |
| `help`       | string  | optional | One-line description. |
| `attributes` | array   | optional | Each entry is an `AttributeSpec`: `name`, `data_type`, `role` (required) plus optional `default_value`. Same shape as graph-file attributes minus `value` and `stages`. |

Diagnostics emitted by `deserialize_registry`:

- `SchemaError` -- missing required field, malformed entry. The bad entry is skipped.
- `DuplicateTypeName` -- same `type` name appears twice. First wins.

Unlike graph files, registry files don't reference each other --
they're flat declarative descriptors.

## Example

A motor-control snippet:

```json
{
    "version": 2,
    "nodes": [
        {
            "id": 1, "type": "Bus", "name": "main_bus", "stage": "control",
            "pos": [100, 100],
            "attrs": [
                {"name": "torque_cmd",  "data_type": "vec3", "role": "output", "stages": ["control"]},
                {"name": "torque_meas", "data_type": "vec3", "role": "input",  "stages": ["feedback"]},
                {"name": "gain",        "data_type": "float", "role": "member", "value": "0.5"}
            ]
        },
        {
            "id": 2, "type": "LowPass", "name": "lowpass", "stage": "feedback",
            "pos": [250, 100],
            "attrs": [
                {"name": "in",     "data_type": "vec3",  "role": "input"},
                {"name": "out",    "data_type": "vec3",  "role": "output"},
                {"name": "cutoff", "data_type": "float", "role": "member", "value": "10.0"}
            ]
        }
    ],
    "links": [
        {"id": 1, "from": {"node": 1, "attr": "torque_cmd"}, "to": {"node": 2, "attr": "in"},          "data_type": "vec3"},
        {"id": 2, "from": {"node": 2, "attr": "out"},        "to": {"node": 1, "attr": "torque_meas"}, "data_type": "vec3"}
    ],
    "stages": [
        {"name": "control",  "color": "#FF0000FF"},
        {"name": "feedback", "color": "#00FF00FF"}
    ],
    "modes": [
        {
            "name": "default", "is_default": true,
            "per_node": [
                {"node": 1, "label": "enable"},
                {"node": 2, "label": "enable"}
            ]
        }
    ]
}
```
