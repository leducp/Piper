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

The canonical tag for a C++ type is `piper::data_type_string<T>()`
(`core/include/piper/builtin_types.h`). The same string is used in
`AttributeSpec::data_type`, in the saved JSON, and inside the `<...>`
of a Step type name, so a node type and its step factory cannot
disagree on spelling.

| Tag           | Use                                  |
|---            |---                                   |
| `float`       | 32-bit real                          |
| `double`      | 64-bit real                          |
| `int8_t` … `int64_t`   | signed integers, 8/16/32/64 bit |
| `uint8_t` … `uint64_t` | unsigned integers, 8/16/32/64 bit |
| `bool`        | true/false                           |
| `string`      | text member (node names, mode labels) |
| `vec2<float>` | 2-component float vector             |
| `vec3<float>` | 3-component float vector (positions, torques, joint targets) |

`data/theme.json` ships colors for every tag above, grouped by family
-- blues for the reals, ambers for signed integers, greens for
unsigned -- with the ramp inside each family tracking width. It also
keeps `vec4`, `mat3x3`, `mat4x4` and `quat` for catalogs that use
them.

Anything else is a custom tag: the engine gets it verbatim and the
editor picks a deterministic pastel by hashing the tag name. Naming
it in `data/theme.json` gives it a stable, chosen color instead --
a data edit, not a code change.

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

Registered by `piper::register_builtin_nodes` (in
`core/src/builtin_nodes.cc`). Most families are instantiated for every
scalar in `piper::BuiltinScalars` -- `float`, `double`, and the signed
and unsigned 8/16/32/64-bit integers -- so the type name is
`family<tag>`, e.g. `add<uint32_t>` or `clamp<double>`.

| Family            | Library | Category   | Instantiated for | Description |
|---                |---      |---         |---               | ---         |
| `constant`        | math    | constant   | all scalars + vectors | Member-valued source |
| `add` / `subtract`| math    | arithmetic | all scalars + vectors | Sum / difference |
| `multiply`        | math    | arithmetic | all scalars      | Product |
| `abs`             | math    | arithmetic | signed scalars   | Absolute value |
| `sin_wave`        | math    | generator  | `float`, `double`| Sine wave (frequency/amplitude/phase) |
| `random`          | math    | generator  | --               | Uniform random float (seed/min/max) |
| `low_pass`        | math    | filter     | `float`, `double`| First-order low-pass (cutoff member) |
| `cast`            | math    | convert    | every ordered scalar pair | `cast<From,To>`, `static_cast` semantics |
| `mux3`            | control | control    | all scalars      | 3-input mux, `int32_t` selector |
| `clamp`           | control | control    | all scalars      | Saturate to [min, max] |
| `preset3`         | control | control    | all scalars      | 3-slot mode-keyed value bank |
| `pid`             | control | control    | `float`, `double`| Discrete PID |
| `external_input`  | io      | external   | all scalars      | Host-written source, `Engine::input<T>` |
| `external_output` | io      | external   | all scalars      | Host-read sink, `Engine::output<T>` |
| `probe`           | example | probe      | all scalars      | Inspection sink (no engine impl) |

`abs` is skipped for unsigned types (it would be the identity), and
the time-stepped families are float-domain only. `cast` is named by
both ends because the destination alone stops identifying the
conversion once there is more than one source type.

Engines (motor control, signal processing, etc.) ship their own
additional types either in code or via `v2::deserialize_registry`
against an engine-supplied catalog file.

## Adding a scalar type

Add the tag declaration and the list entry in
`core/include/piper/builtin_types.h`:

```cpp
PIPER_DECLARE_DATA_TYPE_TAG(__fp16, "float16");

using BuiltinScalars = TypeList<float, double, /* ... */, __fp16>;
```

`register_builtin_nodes` and `register_builtin_steps` are folds over
that list, so both registries pick the type up together. The
`RegistryParity` suite (`tests/engine/registry_parity-t.cc`) fails if
a family is ever added to one side and not the other.

A step whose T is not in the list still works -- declare a tag for it
and instantiate the step template directly. `type_suffix<T>()` builds
the name from the same tag.

## Adding a node type

In code, append to `register_builtin_nodes`:

```cpp
NodeType nt;
nt.type     = "servo_trim";
nt.category = "control";
nt.help     = "Per-axis trim offset applied to a servo command";
nt.attributes = {
    { "command", "float", AttributeSpec::Role::Input,  ""    },
    { "out",     "float", AttributeSpec::Role::Output, ""    },
    { "trim",    "float", AttributeSpec::Role::Member, "0.0" },
};
reg.add("control", nt);   // first arg is the library tag
```

The library is a registration-time argument, not a `NodeType` field:
the editor uses it to group the palette, and runtime users ignore it.

For external catalogs, write the same `NodeType` shape in JSON and
load via `v2::deserialize_registry` -- see `docs/v2_format.md`.

`category` is a `/`-delimited path that lays out the "Add node"
context menu. A single segment (`"control"`) is one submenu; nesting
segments (`"hal/motor"`) builds the submenu tree, letting a project
group its own node pack however it likes:

```cpp
nt.category = "my_project/hal/motor";   // Add node > my_project > hal > motor
```

An empty `category` puts the type at the menu root.

## Drift detection

When a graph saved against an older registry is loaded against a newer
one (or vice versa), `v2::deserialize` walks every saved attribute
against the current spec and emits diagnostics:

- `AttributeMissing` -- saved attribute is no longer in the spec.
- `AttributeAdded`   -- spec has an attribute the saved graph doesn't.
- `AttributeDrift`   -- same name, different `data_type`.

The graph still loads with verbatim saved data; the editor surfaces
the diagnostics in a problems panel for the user to address.
