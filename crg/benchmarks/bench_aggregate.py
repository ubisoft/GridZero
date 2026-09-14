#!/usr/bin/env python3

# Copyright (c) Ubisoft. All Rights Reserved.
# Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

# =============================================================================
# CRG CROSS-MACHINE BENCHMARK AGGREGATOR
# =============================================================================
# Merges the bench_<hostname>_<timestamp>.json sidecars produced by
# bench_runner.py (one per machine) into a single comparison report.
#
#   python bench_aggregate.py                     # glob *.json in cwd
#   python bench_aggregate.py --dir results/       # glob a specific dir
#   python bench_aggregate.py a.json b.json c.json # explicit file list
#
# Output: benchmarks_aggregate.md
# =============================================================================

import argparse
import glob
import json
import os


COMPARISON_PAIRS = [
    ("BM_OOP_FindOnly",   "BM_DOD_FindOnly",   "Find (OOP vs DOD)"),
    ("BM_OOP_InvokeOnly", "BM_DOD_InvokeOnly", "Invoke (OOP vs DOD)"),
    ("BM_OOP_FullChain",  "BM_DOD_FullChain",  "Full chain (OOP vs DOD)"),
    ("BM_LastCall_VCall",   "BM_LastCall_FnPtr",   "Last call (vcall vs fnptr)"),
    ("BM_Guaranteed_VCall", "BM_Guaranteed_FnPtr", "Guaranteed dispatch (vcall vs fnptr)"),
    ("BM_Optional_VCall",   "BM_Optional_FnPtr",   "Optional dispatch (vcall vs fnptr)"),
]


def load_sidecars(paths):
    sidecars = []
    for p in paths:
        with open(p, "r", encoding="utf-8") as f:
            data = json.load(f)
        data["_source_file"] = p
        sidecars.append(data)
    return sidecars


def ns_per_call(b):
    """cpu_time + time_unit is the reliable field pair — bench_runner.py forces
    --benchmark_time_unit=ns at capture time, but scale defensively anyway in
    case a sidecar was produced by an older/different runner invocation."""
    unit = b.get("time_unit", "ns")
    t = b.get("cpu_time")
    if not isinstance(t, (int, float)):
        return None
    scale = {"ns": 1.0, "us": 1e3, "ms": 1e6, "s": 1e9}.get(unit, 1.0)
    return t * scale


def ns_lookup(sidecar):
    out = {}
    for b in sidecar.get("benchmarks", []):
        name = b.get("name")
        ns = ns_per_call(b)
        if name and isinstance(ns, (int, float)):
            out[name] = ns
    return out


def aggregate_scale(b):
    """Google Benchmark's threaded real_time is the wall-clock time divided by
    threads() a second time (it's summed thread-side, then divided once in
    DoNIterations, then divided again by report.iterations in
    GetAdjustedRealTime -- see benchmark_runner.cc/reporter.cc), while
    cpu_time is only divided the second time. bytes_per_second/items_per_second
    are derived from cpu_time, so they read as a per-thread-equivalent rate,
    not the true wall-clock aggregate across all threads. cpu_time/real_time
    recovers the missing threads() factor; for single-threaded benchmarks the
    ratio is ~1 so this is a no-op."""
    rt = b.get("real_time")
    ct = b.get("cpu_time")
    if isinstance(rt, (int, float)) and isinstance(ct, (int, float)) and rt > 0:
        return ct / rt
    return 1.0


def throughput_lookup(sidecar):
    out = {}
    for b in sidecar.get("benchmarks", []):
        name = b.get("name")
        bps = b.get("bytes_per_second")
        ips = b.get("items_per_second")
        scale = aggregate_scale(b)

        val_str = None
        if isinstance(bps, (int, float)) and bps > 0:
            # Convert to Gio/s (GiB/s) -> 1024^3
            val_str = f"{bps * scale / (1024**3):.2f} Gio/s"
        elif isinstance(ips, (int, float)) and ips > 0:
            # Convert to Millions/s
            val_str = f"{ips * scale / 1e6:.1f} M/s"

        if name and val_str:
            out[name] = val_str
    return out


def machine_label(sidecar):
    m = sidecar.get("machine", {})
    return f"{m.get('hostname', '?')} ({m.get('os', '?')}, {m.get('cpu', '?')}, {m.get('config', 'default')})"


def render_machines_table(sidecars):
    lines = ["## Machines", "", "| # | Hostname | OS | CPU | Cores | RAM | Compiler | Config | Revision |",
             "|---|---|---|---|---|---|---|---|---|"]
    for i, s in enumerate(sidecars, 1):
        m = s.get("machine", {})
        lines.append(
            f"| {i} | {m.get('hostname','?')} | {m.get('os','?')} | {m.get('cpu','?')} | "
            f"{m.get('cpu_count','?')} | {m.get('ram_gb','?')} | {m.get('compiler','?')} | "
            f"{m.get('config','default')} | {m.get('revision','?')} |"
        )
    lines.append("")
    return lines


def render_benchmark_table(sidecars):
    all_names = sorted({name for s in sidecars for name in ns_lookup(s)})
    labels = [f"M{i+1}" for i in range(len(sidecars))]

    lines = ["## ns/call by benchmark", "",
             "_Columns are machines in the order listed above (M1, M2, ...)._", "",
             "| Benchmark | " + " | ".join(labels) + " |",
             "|---" * (1 + len(sidecars)) + "|"]

    per_machine = [ns_lookup(s) for s in sidecars]
    for name in all_names:
        row = [name]
        for pm in per_machine:
            v = pm.get(name)
            row.append(f"{v:.3f}" if isinstance(v, (int, float)) else "n/a")
        lines.append("| " + " | ".join(row) + " |")
    lines.append("")
    return lines


def render_throughput_table(sidecars):
    per_machine = [throughput_lookup(s) for s in sidecars]
    all_names = sorted({name for pm in per_machine for name in pm})
    labels = [f"M{i+1}" for i in range(len(sidecars))]

    lines = ["## Throughput (Memory Bound verification)", "",
             "_Measures bandwidth for bulk operations. Displays Gio/s or Millions of items/s (M/s)._", "",
             "| Benchmark | " + " | ".join(labels) + " |",
             "|---" * (1 + len(sidecars)) + "|"]

    if not all_names:
        lines.append("| (No throughput data recorded in these benchmarks) | " + 
                     " | ".join(["n/a"] * len(sidecars)) + " |")
        lines.append("")
        return lines

    for name in all_names:
        row = [name]
        for pm in per_machine:
            row.append(pm.get(name, "n/a"))
        lines.append("| " + " | ".join(row) + " |")
    lines.append("")
    return lines


def dram_tail_bps(sidecar, base_name):
    """Among benchmarks named `base_name/<N>`, return bytes_per_second for the
    largest N — the DRAM-saturated point the sweep was pushed to. Returns None
    if this benchmark family isn't present in the sidecar (e.g. an older run,
    or a machine that skipped the memory-ceiling filter)."""
    best_n, best_bps = None, None
    prefix = base_name + "/"
    for b in sidecar.get("benchmarks", []):
        name = b.get("name", "")
        if not name.startswith(prefix):
            continue
        digits = ""
        for ch in name[len(prefix):]:
            if not ch.isdigit():
                break
            digits += ch
        if not digits:
            continue
        bps = b.get("bytes_per_second")
        if not isinstance(bps, (int, float)):
            continue
        n = int(digits)
        if best_n is None or n > best_n:
            best_n, best_bps = n, bps
    return best_bps


def _gib(bps):
    return f"{bps / (1024**3):.2f} GiB/s" if isinstance(bps, (int, float)) and bps > 0 else "n/a"


# The CRG number the "hardware ceiling" claim is measured against — the
# single-threaded, read-only, DRAM-bound arena sweep (see
# routing/dispatch_cell.bench.cpp).
CEILING_CRG_BENCHMARK = "BM_DispatchCell_EmptyFallback"


def render_ceiling_table(sidecars):
    labels = [f"M{i+1}" for i in range(len(sidecars))]

    lines = ["## Hardware ceiling — utilization (Benchmark C)", "",
             "_DRAM-bound tail (largest swept N) per machine. Utilization = CRG Resolve / "
             "Read Stream — the single-threaded, read-only ceiling matching CRG Resolve's "
             "access pattern. memcpy and STREAM Triad are secondary reference bandwidths, "
             "not the ceiling used for the ratio._", "",
             "| Metric | " + " | ".join(labels) + " |",
             "|---" * (1 + len(sidecars)) + "|"]

    read_stream = [dram_tail_bps(s, "BM_MemCeiling_ReadStream") for s in sidecars]
    crg_bw      = [dram_tail_bps(s, CEILING_CRG_BENCHMARK) for s in sidecars]
    memcpy_bw   = [dram_tail_bps(s, "BM_MemCeiling_Memcpy") for s in sidecars]
    triad_bw    = [dram_tail_bps(s, "BM_MemCeiling_Triad") for s in sidecars]

    lines.append("| Read Stream (headline ceiling) | " + " | ".join(_gib(v) for v in read_stream) + " |")
    lines.append("| CRG Resolve | " + " | ".join(_gib(v) for v in crg_bw) + " |")

    util_cells = []
    for rs, crg in zip(read_stream, crg_bw):
        if isinstance(rs, (int, float)) and rs > 0 and isinstance(crg, (int, float)):
            util_cells.append(f"{100.0 * crg / rs:.1f}%")
        else:
            util_cells.append("n/a")
    lines.append("| **Utilization (CRG / Read Stream)** | " + " | ".join(util_cells) + " |")

    lines.append("| memcpy (reference) | " + " | ".join(_gib(v) for v in memcpy_bw) + " |")
    lines.append("| STREAM Triad (reference) | " + " | ".join(_gib(v) for v in triad_bw) + " |")
    lines.append("")
    return lines


def render_speedup_table(sidecars):
    per_machine = [ns_lookup(s) for s in sidecars]
    labels = [f"M{i+1}" for i in range(len(sidecars))]

    lines = ["## OOP vs CRG speedup ratio", "",
              "_slower / faster, per machine — higher is a bigger CRG win._", "",
              "| Comparison | " + " | ".join(labels) + " |",
              "|---" * (1 + len(sidecars)) + "|"]

    any_row = False
    for slow_name, fast_name, label in COMPARISON_PAIRS:
        row = [label]
        row_has_data = False
        for pm in per_machine:
            slow = pm.get(slow_name)
            fast = pm.get(fast_name)
            if isinstance(slow, (int, float)) and isinstance(fast, (int, float)) and fast > 0:
                row.append(f"{slow / fast:.1f}x")
                row_has_data = True
            else:
                row.append("n/a")
        if row_has_data:
            lines.append("| " + " | ".join(row) + " |")
            any_row = True
    if not any_row:
        lines.append("| (no comparison pairs found in the supplied benchmarks) | " +
                      " | ".join(["n/a"] * len(sidecars)) + " |")
    lines.append("")
    return lines


def render_pmu_table(sidecars):
    labels = [f"M{i+1}" for i in range(len(sidecars))]
    all_counters = sorted({k for s in sidecars for k in (s.get("perf_counters") or {})})

    lines = ["## PMU counters", ""]
    if not all_counters:
        lines.append("PMU: n/a for every machine in this set (all non-Linux, or `perf` missing).")
        lines.append("")
        return lines

    lines.append("| Counter | " + " | ".join(labels) + " |")
    lines.append("|---" * (1 + len(sidecars)) + "|")
    for counter in all_counters:
        row = [counter]
        for s in sidecars:
            v = (s.get("perf_counters") or {}).get(counter)
            row.append(str(v) if v is not None else "n/a (non-Linux)")
        lines.append("| " + " | ".join(row) + " |")
    lines.append("")
    return lines


def render_powermetrics_table(sidecars):
    labels = [f"M{i+1}" for i in range(len(sidecars))]
    any_data = any(s.get("powermetrics") for s in sidecars)

    lines = ["## Thermal / P-E cluster (macOS powermetrics)", "",
             "_Captured concurrently with the benchmark run above "
             "(`powermetrics --samplers cpu_power`, 500 ms interval). A large "
             "freq drop peak->tail is thermal or battery-power throttling under "
             "sustained load, not a fixed architectural ceiling — re-check "
             "machine notes (AC vs battery, Low Power Mode) before trusting a "
             "flat frequency here._", ""]

    if not any_data:
        lines.append("n/a for every machine in this set (non-macOS, `powermetrics` "
                      "not found, or not run with `sudo`).")
        lines.append("")
        return lines

    all_clusters = sorted({name for s in sidecars for name in (s.get("powermetrics") or {})})
    lines.append("| Cluster | " + " | ".join(labels) + " |")
    lines.append("|---" * (1 + len(sidecars)) + "|")
    for cluster in all_clusters:
        row = [f"{cluster} freq avg (MHz)"]
        for s in sidecars:
            entry = (s.get("powermetrics") or {}).get(cluster)
            row.append(str(entry["freq_mhz_avg"]) if entry else "n/a")
        lines.append("| " + " | ".join(row) + " |")

        row = [f"{cluster} freq drop peak->tail"]
        for s in sidecars:
            entry = (s.get("powermetrics") or {}).get(cluster)
            row.append(f"{entry['freq_drop_pct_peak_to_tail']}%" if entry else "n/a")
        lines.append("| " + " | ".join(row) + " |")

        row = [f"{cluster} throttling suspected"]
        for s in sidecars:
            entry = (s.get("powermetrics") or {}).get(cluster)
            row.append(("YES" if entry["throttling_suspected"] else "no") if entry else "n/a")
        lines.append("| " + " | ".join(row) + " |")
    lines.append("")
    return lines


def render_scaling_table(sidecars):
    labels = [f"M{i+1}" for i in range(len(sidecars))]
    if not any(s.get("scaling_probe") for s in sidecars):
        return ["## Compile-time scaling probe", "",
                "Not run on any machine in this set (pass `--scaling` to bench_runner.py).", ""]

    lines = ["## Compile-time scaling probe", "",
             "_Isolated compile of the DenseIndexStore scaling TU "
             "(declares domains at N=8..256 models)._", "",
             "| Machine | Compile time (s) | Object size (KB) |",
             "|---|---|---|"]
    for label, s in zip(labels, sidecars):
        sp = s.get("scaling_probe")
        if sp:
            size_kb = f"{sp['object_bytes'] / 1024:.1f}" if sp.get("object_bytes") else "n/a"
            lines.append(f"| {label} | {sp['compile_seconds']} | {size_kb} |")
        else:
            lines.append(f"| {label} | n/a | n/a |")
    lines.append("")
    return lines


def render_legend(sidecars):
    lines = ["## Legend", ""]
    for i, s in enumerate(sidecars, 1):
        lines.append(f"- M{i} = {machine_label(s)}  (`{s['_source_file']}`)")
    lines.append("")
    return lines


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("files", nargs="*", help="explicit sidecar .json paths")
    ap.add_argument("--dir", default="results/", help="directory to glob bench_*.json from")
    ap.add_argument("--out", default="benchmarks_aggregate.md")
    args = ap.parse_args()

    paths = args.files or sorted(glob.glob(os.path.join(args.dir, "bench_*.json")))
    if not paths:
        print(f"No bench_*.json files found in {args.dir!r} and none given explicitly.")
        return

    sidecars = load_sidecars(paths)

    out_lines = ["# CRG Cross-Machine Benchmark Aggregate", ""]
    out_lines += render_machines_table(sidecars)
    out_lines += render_benchmark_table(sidecars)
    out_lines += render_throughput_table(sidecars)
    out_lines += render_ceiling_table(sidecars)
    out_lines += render_speedup_table(sidecars)
    out_lines += render_pmu_table(sidecars)
    out_lines += render_powermetrics_table(sidecars)
    out_lines += render_scaling_table(sidecars)
    out_lines += render_legend(sidecars)

    with open(args.out, "w", encoding="utf-8") as f:
        f.write("\n".join(out_lines))

    print(f"OK -> {args.out} ({len(sidecars)} machine(s) merged)")


if __name__ == "__main__":
    main()
