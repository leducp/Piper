#!/usr/bin/env python3
"""Plot am_radio CSV output: one row per probe, time + frequency.

Usage:
    ./build/examples/am_radio/am_radio                # writes ./am_radio.csv
    python examples/am_radio/plot.py am_radio.csv     # writes ./am_radio.png
"""
import argparse
import csv
import os
import sys

# Pin the non-interactive Agg backend before pyplot probes Tk/Qt.
os.environ.setdefault("MPLBACKEND", "Agg")

import numpy as np
import matplotlib.pyplot as plt


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
    t = data[:, 1]
    probes = header[2:]
    if not probes:
        sys.exit(f"plot: {path} has no probe columns")
    values = {name: data[:, 2 + i] for i, name in enumerate(probes)}
    return t, values


def plot(t, values, out_path):
    n = len(t)
    dt = float(t[1] - t[0])
    freqs = np.fft.rfftfreq(n, d=dt)

    fig, axes = plt.subplots(len(values), 2, figsize=(11, 3 * len(values)),
                             squeeze=False)
    for i, (name, y) in enumerate(values.items()):
        ax_t, ax_f = axes[i]
        ax_t.plot(t, y, lw=1.0)
        ax_t.set_xlabel("t [s]")
        ax_t.set_ylabel(name)
        ax_t.set_title(f"{name} (time)")
        ax_t.grid(True, alpha=0.3)

        mag = np.abs(np.fft.rfft(y)) / n * 2.0
        peak = float(mag.max())
        ax_f.set_xlabel("f [Hz]")
        ax_f.set_ylabel(f"|{name}|")
        ax_f.set_title(f"{name} (FFT)")
        ax_f.grid(True, alpha=0.3, which="both")
        if peak > 0.0:
            # Markers make single-bin peaks (1 Hz envelope, 200 Hz
            # carrier sidebands) visible at the 0-500 Hz x range.
            ax_f.semilogy(freqs, mag, lw=0.6, marker=".", markersize=2.5)
            ax_f.set_ylim(peak * 1e-4, peak * 1.5)
        else:
            ax_f.plot(freqs, mag, lw=0.6)
            ax_f.set_ylim(-1.0, 1.0)

    fig.tight_layout()
    fig.savefig(out_path, dpi=120)
    print(f"plot: wrote {out_path}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("csv", help="path to the am_radio CSV")
    ap.add_argument("-o", "--out", help="output PNG path (default: <csv>.png)")
    args = ap.parse_args()

    out = args.out or os.path.splitext(args.csv)[0] + ".png"
    t, values = load(args.csv)
    plot(t, values, out)


if __name__ == "__main__":
    main()
