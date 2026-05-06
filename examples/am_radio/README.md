# am_radio

A second engine demo: a textbook AM radio chain — modulator + noisy
channel + envelope-detector receiver — with one probe per measurement
point so the time/freq plot tells the whole story.

## Pipeline

```
(source)                 (process)                            (probe)
                                                              probe_envelope
envelope_gen (5 Hz) ──┐
                      └─► envelope_sum ─┐                     probe_modulated
offset (1.0)        ──┘                 │
                                        ▼
                                     modulator ───────────┐   probe_transmitted
carrier_gen (200 Hz) ──────────────────►                  │
                                                          ▼
noise_gen (uniform) ─────────────────────────────────► channel
                                                         │
                                                         ▼
                                                     rectifier ─► probe_rectified
                                                         │
                                                         ▼
                                                   demod_filter ─► probe_recovered
                                                   (low_pass, fc=20 Hz)
```

`probe_carrier` taps `carrier_gen.out` directly. Six probes total, one
per stage of the chain.

Stages: `source / process / probe`. Open-loop DAG, so multi-stage is
purely visual (see `docs/architecture.md` "Engine execution model").

## Run

```
cmake --build build -j
./build/examples/engine_demo/engine_demo \
    examples/am_radio/am_radio.piper am_radio.csv
python examples/engine_demo/plot.py am_radio.csv
```

`engine_demo` records every `external_output<float>` it finds, so the
same binary handles this pipeline without modification. The CSV gets
6 probe columns; `plot.py` renders one row per column with time
series on the left and FFT magnitude (log y) on the right.

## What to look for

- **probe_envelope**: 1 + 0.5·sin(2π·5·t). Oscillates between 0.5 and
  1.5 at 5 Hz.
- **probe_carrier**: 200 Hz sine. FFT shows a single clean spike at
  200 Hz (and its negative-frequency mirror beyond Nyquist).
- **probe_modulated**: AM signal. FFT shows the carrier at 200 Hz
  flanked by sidebands at 200 ± 5 Hz — the textbook AM spectrum.
- **probe_transmitted**: same as modulated but with the noise floor
  raised by the additive uniform noise.
- **probe_rectified**: full-wave-rectified transmitted signal. FFT
  shows DC, the recovered envelope (5 Hz), and harmonics of 2·fc
  (400 Hz, 800 Hz, ...) from the rectification.
- **probe_recovered**: rectified signal after a 20 Hz low-pass.
  The 400 Hz harmonic is gone, the 5 Hz envelope is preserved.
  Time-domain shows the recovered envelope sitting on a small DC
  offset from the rectification.

## Regenerating the .piper

The committed `am_radio.piper` was produced by `build_pipeline.py`
via the python bindings. To re-generate after editing the topology:

```
cmake --build build --target piper_py -j
PYTHONPATH=build/py_bindings python examples/am_radio/build_pipeline.py
```

The generator is the source of truth; commit both the script and the
resulting `.piper`. Hand-editing the `.piper` in the editor is fine
for tweaks, but topology changes belong in the script.
