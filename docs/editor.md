# Work in the editor

Use the [PID demo](index.md#open-the-pid-demo) for these exercises. Choose
**File → Save As...** and work in a copy so you can return to the bundled
example later. Press **F** with the canvas focused to fit the graph. Use the
**stage** dropdown to highlight one stage at a time, and select **(all)** to
see the whole pipeline.

## Edit a member value

1. Select `kp_bank` and open **Inspector**. Under **Members**, the `value`
   field is the default (`1.0`). The row labeled `tight` overrides it with
   `5.0`.
2. Change the `tight` override to `3.0` and press **Enter**. Run the pipeline
   in the `tight` profile and change `command` in **Live** to see the effect.
   The `loose` mode still uses the default.

To add an override, use **+ value for mode** under the member. Piper starts
the override with the current default. To add another mode to this node,
enter its name under **Modes** and click **Add mode**.

## Choose a mode in a profile

Open **Profiles** and expand `loose`. In the `kp_bank` row, select the `tight`
mode. Activating `loose` now gives `kp_bank` the `tight` value while `ki_bank`
and `kd_bank` still use `loose`. You can also edit a selected node's mode for
the active profile in the **Inspector**.

- **Set active** switches the current profile. The toolbar dropdown does the
  same thing.
- **Set default** chooses the startup profile saved in the pipeline file.
- **Add profile** creates a profile with each node's first mode selected.

## Add an output probe

1. Stop the pipeline. Right-click an empty part of the canvas and choose
   **Add node → external**, then select `external_output<float>`. Select the
   new node and set its **primary** stage to `probe` in the **Inspector**.
2. Drag from `plant.out` to the new node's `in` pin. The existing link to
   `probe_measured` can stay wired to the same output.
3. Under **External I/O** in the **Inspector**, enter `plant_copy` in the
   **External name** field and press **Enter**.
4. Click **Run**. `plant_copy` should show the same value as `probe_measured`
   in **Live**. Use **Problems** to inspect any reported graph issues, then
   save with **Ctrl+S**.

When you need an operation beyond the available node types,
[write a custom step](custom_step.md).
