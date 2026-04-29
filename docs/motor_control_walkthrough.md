# Motor-control walkthrough

This walkthrough opens the two bundled examples in the editor and
takes you through the moving parts: nodes, links, stages, modes, and
the per-pin Bus pattern that lets one node span multiple stages.

Both examples are committed at:

- `examples/motor_control_simple.piper`
- `examples/motor_control_dual_jacobian.piper`

They are also regenerable from C++:

```
./build/tests/fixtures/piper_build_motor_simple        examples/motor_control_simple.piper
./build/tests/fixtures/piper_build_motor_dual_jacobian examples/motor_control_dual_jacobian.piper
```

## Single-channel: `motor_control_simple.piper`

```
sin_wave(joint_target) -> low_pass(filter) -> probe<float>(probe)
```

Open it:

```
./build/app/piper-editor examples/motor_control_simple.piper
```

What to look at:

- **Canvas**. Three nodes wired in a line. Pins are colored by type
  (`float`).
- **Inspector tab** (right panel). Click a node to see its members:
  `sin_wave` exposes `frequency`, `amplitude`, `phase`; `low_pass`
  exposes `cutoff`. Edit one and press Enter -- the change persists
  through `Ctrl+Z`.
- **Stages tab**. Two stages are declared, `control` (red) and
  `feedback` (green). Pick `control` from the toolbar combo: the
  `probe` node dims because it lives in the `feedback` stage.
- **Modes tab**. One profile, `default`, marked default. Each node
  is set to `enable`. Pick a node, switch its label to `disable` --
  body alpha drops; the engine that consumes the JSON would skip
  this node when this profile is active.

This is the minimum useful pipeline: a setpoint generator, a filter,
and a sink for inspection.

## Dual-channel with feedback: `motor_control_dual_jacobian.piper`

```
constant<float>(target_x) ----+
                              +-> jacobian_2x2(jacobian) -> motor(motor_a) -> probe<float>(pose_a)
constant<float>(target_y) ----+                          -> motor(motor_b) -> probe<float>(pose_b)
```

Open it:

```
./build/app/piper-editor examples/motor_control_dual_jacobian.piper
```

The pipeline encodes a small inverse-kinematics-style flow: two
cartesian-space targets feed a 2x2 jacobian whose two outputs drive
two motors. Each motor's measured pose comes back as feedback.

What to look at:

- **Two stages, two roles**. Setpoint flow lives in `control`;
  measured pose lives in `feedback`. Switch between them via the
  toolbar combo or right-click on empty canvas.
- **The Bus pattern**. The `motor` node sits in the `control` stage
  by default, but its `measured` output is tagged with a per-pin
  override of `[feedback]`. Switch the display stage to `feedback`
  and you'll see most of the motor body dim while the `measured`
  pin stays bright -- and vice versa for `control`. This is how V2
  represents a node whose pins live in different stages without
  duplicating the node.
- **Per-stage pipeline DAG**. Inside each stage the dataflow is
  acyclic: `control` is purely setpoint -> jacobian -> command;
  `feedback` is purely motor.measured -> probe. The cycle (motor's
  `command` and `measured` belong to the same physical motor) only
  closes when you read the graph as a whole.

### Try the play button

Hit **play** in the toolbar (between `<` and `>`). The display
stage cycles every 2 seconds, alternating between `control` and
`feedback`. Useful for visualizing a multi-stage pipeline at a
glance.

### Tweaking the example

Open the file, edit, save:

- Click `target_x`, change `value` from `1.0` to `0.5`. `Ctrl+S`
  saves over the example.
- Add a node: right-click empty canvas -> `Add node` -> any type.
  Drag from a pin to wire it in.
- Rename `motor_a`. Press Enter in the inspector's `name` field.
  The header updates immediately.
- Toggle a node's mode: right-click the node -> `Set mode (default)
  -> disable`. Body fades.
- Fan-in is allowed. Drag a second source onto an already-connected
  input. The editor permits it; the engine decides which source
  wins (typically based on stage or mode -- see
  `docs/stages_and_modes.md`).

### Lints

Open the **Problems** tab in the right panel. With the bundled
example loaded cleanly the tab shows `No problems.`. Add a node and
leave it disconnected -- the tab turns orange and lists:

```
* node 'NewNode' is disconnected
* input 'NewNode.in' has no source
```

Click any lint row to jump to the affected node.
