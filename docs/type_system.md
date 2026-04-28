# Piper V2 type system

## Type tags

Pin types are identified by a string tag stored in
`AttributeSpec::data_type` (in the registry) and `Attribute::data_type`
(in graph instances, as a snapshot at save time). The default
type-check policy is exact string equality on the tag -- implemented in
`piper::TypeCheck::compatible`. Subclass `TypeCheck` to relax this
(e.g. allow `int` -> `float` promotion); the policy is plugged into
`piper::validate_connection` at the call site.

## Stock type palette

The default `data/theme.json` ships colors for these tags:

| Tag       | Use                                 |
|---        |---                                  |
| `float`   | scalar real value                    |
| `int`     | scalar integer                       |
| `bool`    | true/false                           |
| `vec2`    | 2-component float vector             |
| `vec3`    | 3-component float vector (positions, torques, joint targets) |
| `vec4`    | 4-component float vector             |
| `mat3x3`  | 3x3 matrix (rotations, jacobians)    |
| `mat4x4`  | 4x4 matrix (poses, transforms)       |
| `quat`    | unit quaternion                      |

Anything else is a custom tag -- the editor renders it with a fallback
color and the engine consuming the V2 JSON gets the tag verbatim.
Adding a new type to the palette is a `data/theme.json` edit, not a
code change.

## Pin roles

Three roles, declared in `AttributeSpec::Role`:

- `Input`  -- receives a value from another node's `Output`.
- `Output` -- produces a value consumable by another node's `Input`.
- `Member` -- per-instance scalar/string the user edits in the
  inspector (PID gains, default sample rate, etc.). Members are not
  link endpoints.

Connecting an `Input` to an `Input`, an `Output` to an `Output`, or
either to a `Member` returns `Connect::KindMismatch` from
`validate_connection`.

## Stages

Stages let a single node participate in multiple execution phases. The
`Bus` pattern is the canonical example: an output pin live only in
stage `"control"` and an input pin live only in stage `"feedback"`,
with the engine resolving direction at runtime.

Two ways to attach a stage:

- **Node-level** -- `Node::stage` applies to every pin by default.
- **Per-pin override** -- `Attribute::stages` lists the stages this
  pin is live in. Empty list = inherit `Node::stage`.

V2 is permissive: `Node::stage` and `Attribute::stages` may reference
a stage name not present in `graph.stages`. Such references surface as
`UnknownStageReference` diagnostics on load; the data is preserved.

## Modes

Mode labels are arbitrary strings on `ModeProfile::per_node` keyed by
`NodeId`. V2 ships `"enable"` and `"disable"` as built-ins; any other
label is opaque to V2 -- the host application's `mode_color_table`
(loaded from `data/theme.json`'s `modes` block) maps custom labels to
RGBA colors. The pipeline engine consuming V2 output decides what each
label means at runtime.

## Stock node types

Registered by `piper::register_builtin_nodes` (in `core/src/builtin_nodes.cc`):

| Type           | Library | Category   | Description                |
|---             |---      |---         |---                         |
| `SinWave`      | math    | generator  | Sine wave generator (frequency/amplitude/phase) |
| `Random`       | math    | generator  | Uniform random float (seed/min/max) |
| `Add`          | math    | arithmetic | Sum of two floats |
| `LowPass`      | math    | filter     | First-order low-pass (cutoff member) |
| `CastFloatInt` | math    | convert    | Truncate float to int |
| `CastIntFloat` | math    | convert    | Promote int to float |
| `ProbeFloat`   | io      | probe      | Float inspection sink |
| `ProbeInt`     | io      | probe      | Int inspection sink |

This list is V2's parity baseline with V1. Engines (motor control,
signal processing, etc.) ship their own additional types either in
code or via `v2::deserialize_registry` against an engine-supplied
catalog file.

## Adding a node type

In code, append to `register_builtin_nodes`:

```cpp
NodeType nt;
nt.type     = "PID";
nt.library  = "control";
nt.category = "control";
nt.help     = "Proportional-integral-derivative controller";
nt.attributes = {
    { "setpoint", "float", AttributeSpec::Role::Input,  ""    },
    { "measured", "float", AttributeSpec::Role::Input,  ""    },
    { "out",      "float", AttributeSpec::Role::Output, ""    },
    { "kp",       "float", AttributeSpec::Role::Member, "1.0" },
    { "ki",       "float", AttributeSpec::Role::Member, "0.0" },
    { "kd",       "float", AttributeSpec::Role::Member, "0.0" },
};
reg.add(nt);
```

For external catalogs, write the same `NodeType` shape in JSON and
load via `v2::deserialize_registry` -- see `docs/v2_format.md`.

## Drift detection

When a graph saved against an older registry is loaded against a newer
one (or vice versa), `v2::deserialize` walks every saved attribute
against the current spec and emits diagnostics:

- `AttributeMissing` -- saved attribute is no longer in the spec.
- `AttributeAdded`   -- spec has an attribute the saved graph doesn't.
- `AttributeDrift`   -- same name, different `data_type`.

The graph still loads with verbatim saved data; the editor surfaces
the diagnostics in a problems panel for the user to address.
