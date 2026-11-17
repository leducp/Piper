#!/usr/bin/env python3
"""Plot pid_demo CSV output: one row per probe, time domain only.
Each panel is shaded by the active profile (column "profile").

Closed-loop step responses are transient, so an FFT just shows
1/f-ish content that doesn't help reading the demo -- this script
keeps only the time view.

Usage:
    ./build/examples/pid_demo/pid_demo               # writes ./pid_demo.csv
    python examples/pid_demo/plot.py pid_demo.csv    # writes ./pid_demo.png
"""
import argparse
import csv
import os
import sys

# Pin the non-interactive Agg backend before pyplot probes Tk/Qt.
os.environ.setdefault("MPLBACKEND", "Agg")

import numpy as np
import matplotlib.pyplot as plt


# Distinguishable, low-saturation backgrounds per profile index.
PROFILE_SHADES = [
    "#dbe4ff", "#ffd6d6", "#d6f0d6", "#fff0c8",
    "#e3d6f0", "#d6f0ee", "#f0e0d0", "#e0e0e0",
]


def load(path):
    with open(path, newline="") as f:
        reader = csv.reader(f)
        header = next(reader)
        rows = [[float(x) for x in row] for row in reader]
    data = np.array(rows)
    if data.size == 0:
        sys.exit(f"plot: {path} has no samples")
    if header[:2] != ["tick", "t"]:
        sys.exit(f"plot: unexpected header {header!r}")
    if "profile" not in header:
        sys.exit(f"plot: {path} has no 'profile' column (run pid_demo to "
                 "generate it)")
    t    = data[:, 1]
    profile = data[:, header.index("profile")].astype(int)
    probe_names = [h for h in header[2:] if h != "profile"]
    if not probe_names:
        sys.exit(f"plot: {path} has no probe columns")
    values = {name: data[:, header.index(name)] for name in probe_names}
    return t, values, profile


def shade_profiles(ax, t, profile):
    boundaries = [0]
    for i in range(1, len(profile)):
        if profile[i] != profile[i - 1]:
            boundaries.append(i)
    boundaries.append(len(profile))
    seen = set()
    for k in range(len(boundaries) - 1):
        i0 = boundaries[k]
        i1 = boundaries[k + 1] - 1
        m  = int(profile[i0])
        color = PROFILE_SHADES[m % len(PROFILE_SHADES)]
        label = None
        if m not in seen:
            label = f"profile {m}"
            seen.add(m)
        ax.axvspan(t[i0], t[i1], color=color, alpha=0.5, label=label, zorder=0)


def plot(t, values, profile, out_path):
    fig, axes = plt.subplots(len(values), 1, figsize=(11, 2.5 * len(values)),
                             squeeze=False, sharex=True)
    for i, (name, y) in enumerate(values.items()):
        ax = axes[i, 0]
        shade_profiles(ax, t, profile)
        ax.plot(t, y, lw=1.0, zorder=2)
        ax.set_ylabel(name)
        ax.grid(True, alpha=0.3)
        if i == 0:
            ax.legend(loc="upper right", fontsize="x-small")
    axes[-1, 0].set_xlabel("t [s]")

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"plot: wrote {out_path}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("csv", help="path to the pid_demo CSV")
    ap.add_argument("-o", "--out", help="output PNG path (default: <csv>.png)")
    args = ap.parse_args()

    out = args.out or os.path.splitext(args.csv)[0] + ".png"
    t, values, profile = load(args.csv)
    plot(t, values, profile, out)


if __name__ == "__main__":
    main()
