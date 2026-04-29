# piper-editor

The Piper V2 GUI editor. Built on top of `piper_canvas` (rendering
and interaction) and `piper_core` (graph data model, V2
serializer, command stack). No engine -- the editor produces V2
JSON that an external engine consumes.

## Running

```
./build/app/piper-editor                                 # blank document
./build/app/piper-editor path/to/file.piper              # open a file at startup
```

The editor finds `data/theme.json` automatically when launched from
the project root or from `build/`.

## Layout

```
| File | View | Help |
| stage [combo] [<] [play/stop] [>]    profile [combo]   |
+--------------------------+-------------------------------+
| canvas                   | Inspector / Stages / Modes /  |
|                          | Problems                      |
+--------------------------+-------------------------------+
| <path or (unsaved)>                          N problem(s) |
```

- **Menu bar**. File / View / Help.
- **Toolbar row**. Current display stage with prev / play / next
  controls; current mode profile.
- **Canvas (left)**. The graph. Pan with middle-drag, zoom with
  the wheel, right-click for context menus.
- **Right panel (tabs)**:
  - **Inspector**. Edit selected node's name, stage, mode label,
    and member-attribute values. Edits push through the host's
    command stack so they undo with everything else.
  - **Stages**. Add / remove / reorder stages. Reorder via up/down
    arrows or drag-drop. Set the displayed stage.
  - **Modes**. CRUD on mode profiles. Set the active profile.
    Matrix view at the bottom: rows = nodes, columns = profiles,
    each cell is a node-mode label combo.
  - **Problems**. Lints (live) + load diagnostics. Click a row to
    jump to the affected node. Tab title goes orange when
    non-empty.
- **Status bar**. Current path (or `(unsaved)`) and live problem
  count.

The right panel is resizable (drag the splitter) and foldable
(`View > Toggle right panel` or `Ctrl+B`).

## Mouse

| Action                       | Effect                                |
|---                           |---                                    |
| middle-drag                  | pan the canvas                        |
| wheel                        | zoom around the cursor                |
| left-click on node           | select (replace), shift-click extends |
| left-click + drag on empty   | box-select                            |
| left-drag on selected node   | move selection (multi-node group)     |
| left-drag on a pin           | drag-to-connect; ghost colored by    |
|                              | the host's `Connect` rules            |
| right-click on node          | per-node menu (Set stage, Set mode)   |
| right-click on empty canvas  | Add node + Display stage menus        |

## Keyboard

| Chord                       | Action                                    |
|---                          |---                                        |
| `Delete` / `Backspace`      | delete selected node(s)                   |
| `Ctrl+C` / `Ctrl+V`         | copy / paste selection (preserves cluster |
|                             | shape relative to the cursor)             |
| `Ctrl+Z` / `Ctrl+Shift+Z`   | undo / redo                               |
| `Ctrl+S`                    | save (Save As if no path)                 |
| `Ctrl+Q`                    | quit                                      |
| `Ctrl+B`                    | toggle right panel                        |
| `Ctrl+G`                    | toggle snap-to-grid                       |
| `Alt+Left` / `Alt+Right`    | previous / next stage                     |

All app shortcuts use `ImGui::Shortcut` with `RouteAlways` so
ImGui's default keyboard navigation never claims them first.

## Theme

`data/theme.json` is loaded at startup and watched for changes
once a second. Edit the file (e.g. tweak `types.float` or add a
`mode_colors.passthrough` entry), save, and the canvas restyles
within a frame. See `docs/v2_format.md` for the full schema.

## Examples

`examples/motor_control_simple.piper` and
`examples/motor_control_dual_jacobian.piper` are the bundled
walkthrough graphs -- see `docs/motor_control_walkthrough.md`.

## Architecture

```
        canvas events                edit commands
piper-editor (app/) ----------> CommandStack ----> piper::Graph
       ^                                                |
       |                                                |
       +-- PiperCanvasGraph adapter (mirror) <----------+
                  ^
                  |
           piper::canvas::Editor (rendering, selection, drag)
```

- The canvas framework is **type-agnostic**: it knows about pins,
  nodes, links, and `type_tag` equality. Every domain rule
  (Connect verdicts, mode/stage dimming, type colors) is host
  policy injected through `PiperCanvasGraph`.
- The host (this binary) bridges canvas events into `core`
  commands so the canvas, inspector, and any future scripted edit
  share one undo stack.
- Mode profile edits (mode-label-per-node) currently bypass the
  command stack -- a `SetModeProfileCommand` would close that gap.
