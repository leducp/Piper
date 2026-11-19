# Write a custom step

Have an operation you want on the canvas? Write it as a small C++ step and
wire it up alongside Piper's built-in nodes. Start with the bundled
[gain_demo.cc](assets/gain_demo.cc). It gives you a working step to modify,+a small program to run it, and a pipeline to open in the editor.

You'll send the number `3` through a gain node, then change one setting in
Piper to make the output go from `12` to `15`.

## Run the demo

Use the [native Linux build from the introduction](index.md#build-the-piper-editor).
From the repository root, compile the demo and run it in `build/`.

```bash
c++ -std=c++20 -Icore/include -Iengine/include docs/assets/gain_demo.cc \
    build/engine/libpiper_engine.a build/core/libpiper_core.a -o build/gain_demo
(cd build && ./gain_demo)
```

You should see three results from the same input and the same wiring.

```text
normal -> 6
high_gain -> 12
monitor -> 3
```

`normal` uses the default factor of `2`. `high_gain` selects the node's
`boost` mode, which overrides the factor to `4`. `monitor` selects
`passthrough`, so the input comes out unchanged.

## Change the gain in Piper

The demo writes two files into `build/`. `gain_demo.piper` is the graph.
`gain_nodes.json` is a **node pack** that tells the editor what pins,
members, and modes to show for your custom node. Copy the pack into a
separate editor configuration for this exercise, then open the graph.

```bash
mkdir -p build/gain-editor-config/piper/nodes
cp build/gain_nodes.json build/gain-editor-config/piper/nodes/
XDG_CONFIG_HOME="$PWD/build/gain-editor-config" \
    ./build/app/piper-editor build/gain_demo.piper
```

1. Select `gain` on the canvas. In **Inspector → Members**, find `factor`
   and its `boost` override.
2. Change the `boost` value from `4` to `5` and press **Enter**.
3. Use **File → Save As...** to save a copy as `build/gain-edited.piper`.

Run the edited graph with the same demo program.

```bash
./build/gain_demo build/gain-edited.piper
```

Look for `high_gain -> 15`. You changed how your custom step behaves by
editing the graph. The other two profiles still print `6` and `3`.

## Turn your own operation into a step

### Give it a box on the canvas

Start by naming what you want to wire and what you want to tune. The gain
node needs an `in` pin, an `out` pin, and a `factor` setting. This
`NodeType` gives the editor that description, including the starting factor
and the modes offered on a new node.

```cpp
--8<-- "assets/gain_demo.cc:node_type"
```

### Put the calculation behind the box

Start with `compute()` at the bottom of the example. The whole calculation
is one line. `declare_io()` tells the engine where the two pins connect in
your code, using the same names and types as the node description.
`on_mode_change()` reads the configured factor and prepares it for the next
tick. Piper supplies the override automatically when the selected mode has
one, so the same multiplication also works for `boost`.

```cpp
--8<-- "assets/gain_demo.cc:step"
```

The engine calls `on_mode_change()` during setup to check each available
mode. Use it to prepare settings, as the demo does when it parses the
factor. Put the work that should happen on each tick in `compute()`.

### Connect the description to the implementation

The node registry holds the description. The step registry tells the engine
how to create a `Gain` when it sees a node of that type. Register both under
the same type key, `guide_gain`.

```cpp
--8<-- "assets/gain_demo.cc:register"
```

The demo program is the host for this graph. It loads the `.piper` file,
builds the engine with the step registry, and sends one input through the
graph with `Engine::play()` for each profile. That is why you use the demo
program to run the graph you saved in the editor.

To try your own operation, use `Gain` as a starting point. Decide what goes
in, what comes out, and which settings you want to tune from the Inspector.
Then write the calculation and give the type its own name.

For more detail, see the [node pack format](v2_format.md#registry-catalog-format)
and [host inputs and outputs](architecture.md#external-io).
