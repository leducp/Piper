# piper_canvas

Reusable ImGui-backed node-editor framework. Renders a host-supplied
node graph onto an `ImDrawList`, handles pan/zoom, selection, drag-to-
move, drag-to-connect, copy/paste/delete shortcuts, and a context-menu
hook. The framework owns interaction; the host owns the data.

## Public API surface

Headers in `include/piper/canvas/`. Group by concern:

| Concern              | Headers                                       |
|---                   |---                                            |
| IDs + opaque wrappers| `ids.h`                                       |
| Render descriptors   | `graph.h` (`Pin`, `Node`, `Link`, `Graph`)    |
| Editor entry point   | `editor.h` (`Editor`, `BodyRenderer`, `ContextMenuFn`) |
| Events               | `event.h` (`EventKind`, `Event`)              |
| Style                | `style.h`                                     |
| Math                 | `aabb.h`, `transform.h`, `cull.h`             |
| Hit-testing          | `hit_test.h`                                  |
| Selection            | `selection.h`                                 |
| Logging hook         | `log.h`                                       |

## Layering

`piper_canvas` links only `imgui::imgui`. It does NOT link
`piper_core`, GLFW, or any host dependency. Use it from any project
that drives an ImGui frame. The host writes a thin adapter that
implements `canvas::Graph` over its own node/link storage and
translates events back into mutations.

## Pull for state, push for changes

The framework re-reads `Graph::nodes()` and `Graph::links()` each
frame. Spans returned must remain valid for the duration of the
current `Editor::draw()` call — hosts typically own a stable mirror
vector and update it only in response to consumed events.

Mutations are emitted as `Event`s and drained via
`consume_events()`. The framework holds zero durable state beyond
ephemeral interaction (selection, drag-in-progress, current popup).
Spans inside `Event` (e.g. `Event::selection`) are valid until the
next `consume_events()` or `draw()` — copy them if you need to retain
beyond the current frame.

## Quick reference

```cpp
#include <piper/canvas/editor.h>
#include <piper/canvas/event.h>
#include <piper/canvas/graph.h>

class HostGraph : public piper::canvas::Graph
{
public:
    std::span<piper::canvas::Node const> nodes() const override { return nodes_; }
    std::span<piper::canvas::Link const> links() const override { return links_; }

    // Optionally override can_connect to plug in your domain rules.
    Connect can_connect(Pin const& a, Pin const& b) const override
    {
        if (a.type_tag != b.type_tag) { return Connect::TypeMismatch; }
        // SameNode and KindMismatch are short-circuited by the editor
        // before reaching here.
        return Connect::Allow;
    }

    void apply(piper::canvas::Event const& ev) { /* ... */ }

private:
    std::vector<piper::canvas::Node> nodes_;
    std::vector<piper::canvas::Link> links_;
};

// Each ImGui frame, inside your own Begin/End:
HostGraph              graph;
piper::canvas::Editor  editor{graph};

editor.draw(ImGui::GetContentRegionAvail());
for (auto const& ev : editor.consume_events())
{
    graph.apply(ev);
}
```

## Interaction reference

| Input                          | Effect                                    |
|---                             |---                                        |
| Middle-drag                    | Pan                                       |
| Mouse wheel                    | Zoom around cursor                        |
| Left-click on node             | Select (replace)                          |
| Shift + left-click on node     | Toggle selection                          |
| Left-drag on selected node     | Move selection (emits `NodeMoved`)        |
| Left-click + drag on empty     | Box-select (shift extends)                |
| Left-drag on pin               | Drag-to-connect; ghost colored by `Connect`|
| Right-click                    | `ContextMenuRequested` + popup callback   |
| `Delete` / `Backspace`         | `NodeDeleted` per selected node           |
| `Ctrl+C` / `Ctrl+V`            | `CopyRequested` / `PasteRequested`        |

## Events

`Event::kind`:

| Kind                   | Populated fields                               |
|---                     |---                                             |
| `NodeMoved`            | `node`, `pos`                                  |
| `NodeDeleted`          | `node`                                         |
| `LinkCreated`          | `pin_from` (output), `pin_to` (input)          |
| `LinkDeleted`          | `link`                                         |
| `SelectionChanged`     | `selection`                                    |
| `ContextMenuRequested` | `node` (or `invalid_node_id`), `pos`           |
| `CopyRequested`        | `selection`                                    |
| `PasteRequested`       | `pos` (cursor in canvas-space)                 |

## Hooks

- `Editor::set_body_renderer` — per-visible-node callback for in-body
  text or widgets, invoked after the body bg/header/outline are
  drawn but before pins.
- `Editor::set_context_menu` — invoked inside an active ImGui popup
  on right-click. Host adds `MenuItem`/`Selectable` calls.
- `set_log_sink` (`log.h`) — receives `(LogLevel, std::string_view)`
  for diagnostic messages. Default sink is no-op.

## Demo

`examples/canvas_demo` is a standalone GLFW + ImGui app that drives
the framework over a small `DemoGraph`. It exercises every event
kind: drag-to-move, drag-to-connect (with `TypeMismatch`, `SameNode`
and `KindMismatch` feedback), copy/paste of multi-selection with id
regeneration, delete with incident-link cleanup, and a body-renderer
+ context-menu hookup. Fanout (one output → many inputs) and fanin
(many outputs → one input) are both allowed; runtime semantics live
in the engine/stage/mode layer, not the editor.

```
build/examples/canvas_demo/canvas_demo
```

## Tests

Headless coverage in `tests/canvas/piper_canvas_test`:

- `aabb-t`, `transform-t`, `cull-t`, `pin_layout-t`, `link_routing-t`
- `hit_test-t` (node + pin + bezier)
- `selection-t`

```
cmake --build build
build/tests/canvas/piper_canvas_test
```
