#!/usr/bin/env python3

# Copyright (c) Ubisoft. All Rights Reserved.
# Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

# =============================================================================
# Rules ON/OFF thread-scaling chart, normalized to the machine's own measured
# BM_MemCeiling_ReadStream_ThreadScaling bandwidth per thread count.
#
#   python bench_plot_threadscaling.py <rules_off.json> <rules_on.json> --out <path.png>
# =============================================================================

import argparse
import json

import matplotlib.pyplot as plt
import numpy as np


def smooth(values, window=5):
    y = np.asarray(values, dtype=float)
    pad = window // 2
    ypad = np.pad(y, pad, mode="edge")
    kernel = np.ones(window) / window
    return np.convolve(ypad, kernel, mode="valid")


def aggregate_scale(b):
    rt = b.get("real_time")
    ct = b.get("cpu_time")
    if isinstance(rt, (int, float)) and isinstance(ct, (int, float)) and rt > 0:
        return ct / rt
    return 1.0


def bps_by_threads(sidecar, prefix):
    out = {}
    for b in sidecar.get("benchmarks", []):
        name = b.get("name", "")
        if not name.startswith(prefix + "/") or not name.endswith("_mean"):
            continue
        thread_token = None
        for p in name.split("/"):
            if p.startswith("threads:"):
                thread_token = p[len("threads:"):].split("_")[0]
                break
        if thread_token is None:
            continue
        bps = b.get("bytes_per_second")
        if not isinstance(bps, (int, float)):
            continue
        out[int(thread_token)] = bps * aggregate_scale(b)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rules_off_json")
    ap.add_argument("rules_on_json")
    ap.add_argument("--out", default="threadscaling_ceiling.png")
    args = ap.parse_args()

    with open(args.rules_off_json, "r", encoding="utf-8") as f:
        off = json.load(f)
    with open(args.rules_on_json, "r", encoding="utf-8") as f:
        on = json.load(f)

    off_dispatch = bps_by_threads(off, "BM_DispatchCell_EmptyFallback_ThreadScaling")
    off_ceiling = bps_by_threads(off, "BM_MemCeiling_ReadStream_ThreadScaling")
    on_dispatch = bps_by_threads(on, "BM_DispatchCell_EmptyFallback_ThreadScaling")
    on_ceiling = bps_by_threads(on, "BM_MemCeiling_ReadStream_ThreadScaling")

    threads = sorted(set(off_dispatch) & set(off_ceiling) & set(on_dispatch) & set(on_ceiling))
    off_pct = [100.0 * off_dispatch[t] / off_ceiling[t] for t in threads]
    on_pct = [100.0 * on_dispatch[t] / on_ceiling[t] for t in threads]

    off_smooth = np.minimum(smooth(off_pct), 100.0)
    on_smooth = np.minimum(smooth(on_pct), 100.0)

    bg = "#0d1117"
    fg = "#c0cfe0"
    amber = "#fbbf24"
    blue = "#38bdf8"
    ref = "#a78bfa"
    flag = "#ef4444"

    fig, ax = plt.subplots(figsize=(11, 7.2), facecolor=bg)
    ax.set_facecolor(bg)

    ax.axhspan(95.0, 104.0, facecolor=ref, alpha=0.38, edgecolor=ref,
               linewidth=1.4, zorder=1)
    ax.text(threads[0], 106, "measured memory-bandwidth ceiling ± noise",
            color=fg, fontsize=15, fontweight="bold", ha="left", va="bottom")

    callout_threads = {1, 8, 24}
    callout_idx = [i for i, t in enumerate(threads) if t in callout_threads]
    idx_of = {t: i for i, t in enumerate(threads)}

    ax.plot(threads, on_smooth, color=blue, linewidth=6, solid_capstyle="round", zorder=3)
    ax.plot([threads[i] for i in callout_idx], [on_smooth[i] for i in callout_idx],
            color=blue, marker="o", markersize=16, linewidth=0, zorder=4)

    ax.plot(threads, off_smooth, color=amber, linewidth=6, solid_capstyle="round", zorder=3)
    ax.plot([threads[i] for i in callout_idx], [off_smooth[i] for i in callout_idx],
            color=amber, marker="o", markersize=16, linewidth=0, zorder=4)

    label_bg = dict(facecolor=bg, edgecolor="none", pad=2)

    ax.annotate("32 B/cell", xy=(threads[-1], on_smooth[-1]), xytext=(-14, 16),
                textcoords="offset points", color=blue, fontsize=20,
                fontweight="bold", ha="right", zorder=5, bbox=label_bg)
    ax.annotate("8 B/cell", xy=(threads[-1], off_smooth[-1]), xytext=(-14, 14),
                textcoords="offset points", color=amber, fontsize=20,
                fontweight="bold", ha="right", zorder=5, bbox=label_bg)

    i1, i8 = idx_of[1], idx_of[8]
    ax.annotate(f"{on_pct[i1]:.0f}% @1T", xy=(1, on_smooth[i1]), xytext=(34, 26),
                textcoords="offset points", color=blue, fontsize=13, ha="left",
                zorder=5, bbox=label_bg)
    ax.annotate(f"{on_pct[i8]:.0f}% @8T", xy=(8, on_smooth[i8]), xytext=(0, -26),
                textcoords="offset points", color=blue, fontsize=13, ha="center",
                zorder=5, bbox=label_bg)

    OFF_CELL_BYTES = 8

    def fmt_dispatches(bps):
        return f"{bps / OFF_CELL_BYTES / 1e9:.2f} B/s"

    i24 = idx_of[24]
    ax.annotate(f"{off_pct[i1]:.0f}% @1T\n{fmt_dispatches(off_dispatch[1])}", xy=(1, off_smooth[i1]), xytext=(34, -22),
                textcoords="offset points", color=amber, fontsize=13, ha="left",
                zorder=5, bbox=label_bg)
    ax.annotate(f"{off_pct[i8]:.0f}% @8T\n{fmt_dispatches(off_dispatch[8])}", xy=(8, off_smooth[i8]), xytext=(0, -28),
                textcoords="offset points", color=amber, fontsize=13, ha="center",
                zorder=5, bbox=label_bg)
    ax.annotate(f"{off_pct[i24]:.0f}% @24T\n{fmt_dispatches(off_dispatch[24])}", xy=(24, off_smooth[i24]), xytext=(-14, -26),
                textcoords="offset points", color=amber, fontsize=13, ha="right",
                zorder=5, bbox=label_bg)
    ax.annotate("still climbing →", xy=(24, off_smooth[i24]), xytext=(18, 0),
                textcoords="offset points", color=amber, fontsize=13, ha="left",
                va="center", zorder=5, bbox=label_bg, annotation_clip=False)

    ax.annotate("SATURATED", xy=(8, on_smooth[i8]), xytext=(90, 8),
                textcoords="offset points", color=flag, fontsize=16,
                fontweight="bold", ha="left", va="center", zorder=6, bbox=label_bg,
                arrowprops=dict(arrowstyle="-|>", color=flag, lw=2.2,
                                 shrinkA=4, shrinkB=8))

    ax.set_xlabel("Threads", color=fg, fontsize=16)
    ax.set_ylabel("% of measured bandwidth ceiling", color=fg, fontsize=16)

    ax.set_ylim(5, 112)
    ax.set_xlim(threads[0] - 0.5, threads[-1] + 0.5)
    ax.set_xticks(sorted(callout_threads))
    ax.set_yticks([])
    ax.grid(False)
    ax.tick_params(colors=fg, labelsize=15, length=0)
    for spine in ax.spines.values():
        spine.set_visible(False)

    fig.tight_layout()
    fig.savefig(args.out, dpi=160, facecolor=bg)
    print(f"OK -> {args.out}")
    print("threads:", threads)
    print("Rules OFF %:", [f"{v:.1f}" for v in off_pct])
    print("Rules ON  %:", [f"{v:.1f}" for v in on_pct])


if __name__ == "__main__":
    main()
