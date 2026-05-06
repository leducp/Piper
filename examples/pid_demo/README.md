# pid_demo

Closed-loop PID controller driving a low-pass plant, with mode-keyed
gain banks. The host (the `pid_demo` binary) drives a square-wave
setpoint and switches modes every second so the plot shows three
distinct closed-loop responses side by side.

## Pipeline

```
[control stage]                                        [plant stage]
external_input<float> "command" ──► pid.setpoint
preset3<float> "kp_bank"        ──► pid.kp
preset3<float> "ki_bank"        ──► pid.ki                  ┌────────┐
preset3<float> "kd_bank"        ──► pid.kd       pid.out ──►│ plant  │
                                    pid.measured ◄──────────│low_pass│
                                                            │fc=2 Hz │
[probe stage]                                               └────────┘
probe_command, probe_pid_out, probe_measured
```

Stage order is `control -> plant -> probe`. `pid.measured` is wired
to `plant.out`, but `plant` runs *after* `control`, so each tick the
PID reads the plant's *previous* output -- the n-1 break that closes
the feedback loop without forming a same-stage cycle. See
`docs/architecture.md` "Engine execution model".

## Modes

The three `preset3<float>` banks each hold three slots labeled
`tight / loose / bypass`. Every mode profile labels all three banks
with the same string, so switching modes swaps all three gains
atomically.

| mode    | kp  | ki  | kd  | comment                                       |
|---------|-----|-----|-----|-----------------------------------------------|
| tight   | 5.0 | 2.0 | 0.1 | snappy tracking, small steady error           |
| loose   | 1.0 | 0.2 | 0   | slow tracking, larger overshoot               |
| bypass  | 0.1 | 0.0 | 0   | very weak P, plant dominates                  |

The bundled `pid<T>` step takes the derivative on the *measurement*
(not the error), so a step setpoint does not kick the controller --
nonzero `kd` is safe and just adds damping against the plant's
velocity.

## Run

```
cmake --build build -j
./build/examples/pid_demo/pid_demo                       # writes ./pid_demo.csv
python examples/pid_demo/plot.py pid_demo.csv            # writes ./pid_demo.png
```

The binary drives the host-side `external_input<float>` named
`command` (the setpoint) and switches modes on a timer; the plot
script reads the `mode` column from the CSV and shades each region.

The binary takes optional positional args:

```
pid_demo [pipeline.piper [output.csv]]
```

It runs 4000 ticks (4 s of simulation at the engine's 1 kHz
`tick_period`), driving `command` with a ±1 square wave (period 2 s)
and switching modes every 1 s of sim time:
`tight (0) → loose (1) → bypass (2) → tight (0)`. The CSV gets a
`mode` integer column; `plot.py` shades time regions by mode.

## What to look for

On `probe_measured` (the plant output):

- **tight (mode 0)**: plant rises to ~0.85 of the setpoint inside
  the first ~50 ms after each step, then converges.
- **loose (mode 1)**: visibly slower, settles at a smaller fraction
  of the setpoint within the second.
- **bypass (mode 2)**: plant barely moves; the very low gain barely
  drives the loop.

`probe_pid_out` shows the controller working hardest in `tight` and
almost idle in `bypass`. The discontinuities at each 1-second
boundary on `probe_pid_out` come from the gain change, not from
the setpoint (which only flips at even seconds).

## Regenerating the .piper

```
cmake --build build --target piper_py -j
PYTHONPATH=build/py_bindings python examples/pid_demo/build_pipeline.py
```

The generator is the source of truth; commit both the script and
the resulting `.piper`.
