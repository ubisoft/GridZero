#!/usr/bin/env python3

# Copyright (c) Ubisoft. All Rights Reserved.
# Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

# =============================================================================
# Headline bullet chart for the "does CRG reach the hardware ceiling" slide.
# Row 1 (memory-bound): measured bar + a real hardware ceiling tick.
# Row 2 (call-bound): measured bar only — no hardware floor is benchmarked
# for dispatch latency, so no tick is drawn (a self-set target isn't one).
#
#   python bench_plot_headline_bullets.py --ceiling-gibs 16.74 --resolve-gibs 16.71 \
#       --dispatch-ns 1.151 --aggregate-bps 6.47e9 --out <path.png>
# =============================================================================

import argparse

import matplotlib.pyplot as plt


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ceiling-gibs", type=float, required=True)
    ap.add_argument("--resolve-gibs", type=float, required=True)
    ap.add_argument("--dispatch-ns", type=float, required=True)
    ap.add_argument("--aggregate-bps", type=float, required=True)
    ap.add_argument("--out", default="headline_bullets.png")
    args = ap.parse_args()

    bg = "#0d1117"
    fg = "#c0cfe0"
    amber = "#fbbf24"
    blue = "#38bdf8"
    track = "#1c2530"

    fig, ax = plt.subplots(figsize=(11, 2.6), facecolor=bg)
    ax.set_facecolor(bg)

    bar_h = 0.32
    row1_y, row2_y = 0.95, 0.0

    scale = args.ceiling_gibs * 1.12
    frac_measured = args.resolve_gibs / scale
    frac_limit = args.ceiling_gibs / scale

    ax.barh(row1_y, 1.0, height=bar_h, color=track, zorder=1)
    ax.barh(row1_y, frac_measured, height=bar_h, color=blue, zorder=2)
    ax.plot([frac_limit, frac_limit], [row1_y - bar_h / 2 - 0.04, row1_y + bar_h / 2 + 0.04],
            color=fg, linewidth=3, zorder=3)
    ax.text(frac_measured + 0.015, row1_y, f"{args.resolve_gibs:.2f} GiB/s",
            color="#fff", fontsize=17, fontweight="bold", va="center", ha="left")
    ax.text(frac_measured + 0.015, row1_y - bar_h / 2 - 0.16, f"ceiling {args.ceiling_gibs:.2f} GiB/s",
            color=fg, fontsize=12, va="top", ha="left")
    ax.text(0, row1_y - bar_h / 2 - 0.16, "32 B/cell — memory-bound",
            color=blue, fontsize=13, fontweight="bold", va="top", ha="left")

    frac_measured2 = 0.86
    ax.barh(row2_y, 1.0, height=bar_h, color=track, zorder=1)
    ax.barh(row2_y, frac_measured2, height=bar_h, color=amber, zorder=2)
    ax.text(frac_measured2 + 0.015, row2_y, f"{args.dispatch_ns:.3f} ns/cell",
            color="#fff", fontsize=17, fontweight="bold", va="center", ha="left")
    ax.text(0, row2_y - bar_h / 2 - 0.16, "8 B/cell — call-bound",
            color=amber, fontsize=13, fontweight="bold", va="top", ha="left")

    ax.text(1.0, row2_y - bar_h / 2 - 0.42, "6.47 Billion dispatches/s aggregate @ 24 threads →",
            color=amber, fontsize=16, fontweight="bold", ha="right", va="top")

    ax.set_xlim(0, 1)
    ax.set_xticks([])
    ax.set_yticks([])
    ax.set_ylim(row2_y - bar_h / 2 - 0.62, row1_y + bar_h / 2 + 0.12)
    for spine in ax.spines.values():
        spine.set_visible(False)

    fig.savefig(args.out, dpi=160, facecolor=bg, bbox_inches="tight", pad_inches=0.12)
    print(f"OK -> {args.out}")


if __name__ == "__main__":
    main()
