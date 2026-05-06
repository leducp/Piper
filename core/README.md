# piper_core

Domain layer for Piper V2: graph data model, type checking, V2
serializer, theme loader, command stack. No GUI dependencies. Single-
threaded by default. Exception-light.

## Public API surface

Headers in `include/piper/`. Group by concern:

| Concern              | Headers                                     |
|---                   |---                                          |
| Data model           | `node.h`, `link.h`, `attribute.h`, `node_type.h`, `stage.h`, `mode_profile.h`, `color.h` |
| Graph operations     | `graph.h`, `registry.h`                      |
| Type checking        | `type_check.h`, `connect.h`                  |
| V2 file format       | `serialize_v2.h`, `diagnostic.h`             |
| Theme + builtins     | `theme.h`, `builtin_nodes.h`                 |
| Undo/redo            | `command.h`, `command_stack.h`, `commands.h` |
| Color I/O helpers    | `rgba_io.h`                                  |

## Layering

`piper_core` links only `nlohmann_json::nlohmann_json` (private). It
does NOT link ImGui, GLFW, or any GUI dependency. The `canvas/`
framework (Epic 2) is the inverse: it must NOT link `piper_core`
either -- keeping it domain-agnostic and reusable. The application
shell (Epic 4) is the only target that links both.

## Quick reference

```cpp
#include <piper/builtin_nodes.h>
#include <piper/graph.h>
#include <piper/registry.h>
#include <piper/serialize_v2.h>

// Construct
piper::NodeRegistry reg;
piper::register_builtin_nodes(reg);

piper::Graph g;
auto sin_id = g.add_node(*reg.find("SinWave"), "src", "control", { 0, 0 });

// Validate before linking
piper::TypeCheck tc;
auto verdict = piper::validate_connection(g, from_pin, to_pin, tc);

// Serialize
std::string text = piper::v2::serialize(g);

// Load
auto loaded = piper::v2::deserialize(text, reg);
for (auto const& d : loaded.diagnostics) { /* surface to user */ }

// Edit through commands for undo/redo
piper::CommandStack stack;
stack.push(std::make_unique<piper::MoveNodeCommand>(sin_id, { 100, 0 }), g);
stack.undo(g);
```

## V2 file format

See `docs/v2_format.md` for the authoritative schema reference (graph
files and registry catalog files).

## Adding a new builtin node type

Edit `core/src/builtin_nodes.cc`:

```cpp
NodeType nt;
nt.type     = "MyNode";
nt.library  = "my_library";
nt.category = "filter";
nt.help     = "What MyNode does";
nt.attributes = {
    { "in",     "float", AttributeSpec::Role::Input,  "" },
    { "param",  "float", AttributeSpec::Role::Member, "1.0" },
    { "out",    "float", AttributeSpec::Role::Output, "" },
};
reg.add(nt);
```

For external engines that ship their own catalog, see
`v2::serialize_registry` / `v2::deserialize_registry` --
`docs/v2_format.md` documents the registry JSON schema.

## Tests

Built and run as part of the standard test suite:
- `cmake --build build`
- `cd build && ctest`

Test executable is `build/tests/core/piper_core_test`.
