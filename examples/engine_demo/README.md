# engine_demo

Smallest end-to-end demo of the Piper engine: load a V2 pipeline,
build it, run a fixed number of ticks, dump every probe to CSV, and
plot the result in time + frequency.

## Pipeline

`examples/engine_demo.piper` is a low-pass filter demo:

```
joint_target (sin_wave 1 Hz)  --+
                                +-> add -> probe_raw      (pre-filter)
noise        (sin_wave 100 Hz) -+        \
                                          +-> filter (low_pass, fc=10 Hz)
                                                  -> probe_filtered (post-filter)
```

Three stages (`generate / control / feedback`); see
`docs/architecture.md` for why an open-loop pipeline doesn't strictly
need them.

## Run

```
cmake --build build -j
./build/examples/engine_demo/engine_demo            # writes ./engine_demo.csv (4000 samples)
python examples/engine_demo/plot.py engine_demo.csv # writes ./engine_demo.png
```

The binary takes optional positional args:

```
engine_demo [pipeline.piper [output.csv]]
```

Defaults are the bundled pipeline and `./engine_demo.csv` in the
current directory. Sample rate is the engine's hardcoded
`tick_period = 0.001 s` (1 kHz), so 4000 ticks = 4 s of simulation.

## Reading the plot

`plot.py` lays out one row per probe column with two panels each: time
series on the left, FFT magnitude (log y) on the right.

For the bundled pipeline:

- `probe_raw` shows the 1 Hz signal swamped by the 100 Hz "noise";
  FFT shows two equal-amplitude spikes at 1 Hz and 100 Hz.
- `probe_filtered` shows the recovered 1 Hz signal with a small 100 Hz
  residual; FFT shows the 100 Hz spike attenuated by ~10x, matching a
  first-order low-pass at a decade above cutoff (1/sqrt(101) ~ 0.1).
