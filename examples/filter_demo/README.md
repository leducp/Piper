# filter_demo

Smallest engine demo: a 1 Hz signal mixed with 100 Hz "noise",
filtered by a first-order low-pass.

## Pipeline

```
joint_target (sin_wave 1 Hz)   --+
                                 +-> add -> probe_raw       (pre-filter)
noise        (sin_wave 100 Hz) --+        \
                                           +-> filter (low_pass, fc=10 Hz)
                                                   -> probe_filtered (post-filter)
```

Three stages (`generate / control / feedback`). The split is
pedagogical only -- this is an open-loop DAG, so a single stage would
work. See `docs/architecture.md` "Engine execution model" for when
multiple stages are necessary.

## Run

```
cmake --build build -j
./build/examples/filter_demo/filter_demo                   # writes ./filter_demo.csv
python examples/filter_demo/plot.py filter_demo.csv        # writes ./filter_demo.png
```

## What to look for

- `probe_raw`: the 1 Hz signal swamped by the 100 Hz noise. FFT shows
  two equal spikes at 1 Hz and 100 Hz.
- `probe_filtered`: the recovered 1 Hz signal with a small 100 Hz
  residual. FFT shows the 100 Hz spike attenuated by ~10x, matching
  a first-order low-pass at a decade above cutoff (1/sqrt(101) ~ 0.1).

## Switch cutoff by label

Open `filter_demo.piper` in the editor. Start Run, then select the toolbar's `narrow_band` profile
(`cutoff_10Hz`) or `wide_band` profile (`cutoff_100Hz`). Cutoff changes between
10 Hz and 100 Hz while `dt` is independently stored as 0.001 s in each set.
Switching profiles preserves filter history.

Select the filter and use **Members per label** in the inspector. With the
switch off, all labels use the same values. With it on, every label has its own
complete set immediately, including `enable`, `disable`, `neutral`, and unset.
The inspector displays every label and its editable members together, marking
the active label. Editing a label does not change the selected profile.

The label picker combines built-in labels, node labels, and theme suggestions,
showing each label once. Introduce custom labels in that picker; their complete
sets are initialized automatically. Turning per-label members off keeps the
active label's values. Editing members continues to rebuild the running
engine.
