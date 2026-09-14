#!/usr/bin/env python3

# Copyright (c) Ubisoft. All Rights Reserved.
# Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

# =============================================================================
# CRG CROSS-MACHINE BENCHMARK RUNNER
# =============================================================================
# Self-contained: copy this file (and nothing else) to any machine with a
# C++17 compiler + CMake, run it, and hand back the two files it produces.
#
#   python bench_runner.py                      # configure+build+run, full sweep
#   python bench_runner.py --no-build            # reuse an existing build dir
#   python bench_runner.py --filter 'BM_DOD_.*'  # subset (regex, passed through)
#   python bench_runner.py --scaling             # also time the DenseIndexStore
#                                                 # scaling TU in isolation
#   python bench_runner.py --out-dir results/    # output dir (auto-triggers aggregate)
#
# Output: bench_<hostname>_<timestamp>.md  (human view)
#         bench_<hostname>_<timestamp>.json (merge key for bench_aggregate.py)
#
# On Linux, if `perf` is on PATH, re-runs the OOP-vs-CRG comparison benchmarks
# under `perf stat` and folds IPC / branch-miss / cache-miss counters into the
# same two files. Windows and macOS contribute timing only — this is noted
# explicitly in the output rather than silently omitted.
#
# On macOS, if `powermetrics` is on PATH AND this script is run as root
# (`sudo python3 bench_runner.py ...`), captures P-Cluster/E-Cluster active
# frequency for the duration of the benchmark sweep and reports whether
# frequency collapses over the run — the signature of thermal throttling on
# fanless machines (MacBook Air), which otherwise looks identical to a
# multi-core "hardware ceiling" in the raw items_per_second numbers. Skipped
# with a printed note (not silently) if not root — this script never prompts
# for a password.
# =============================================================================

import argparse
import datetime
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
GRIDZERO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))  # .../gridzero
BIN_NAME = "Run_CRG_Benchmarks.exe" if platform.system() == "Windows" else "Run_CRG_Benchmarks"

# Benchmarks with a direct OOP vs CRG (DOD) counterpart — used for the
# speedup-ratio row in the aggregate report and, on Linux, for the PMU pass.
COMPARISON_PAIRS = [
    ("BM_OOP_FindOnly",   "BM_DOD_FindOnly"),
    ("BM_OOP_InvokeOnly", "BM_DOD_InvokeOnly"),
    ("BM_OOP_FullChain",  "BM_DOD_FullChain"),
    ("BM_LastCall_VCall", "BM_LastCall_FnPtr"),
    ("BM_Guaranteed_VCall", "BM_Guaranteed_FnPtr"),
    ("BM_Optional_VCall",   "BM_Optional_FnPtr"),
]

PERF_EVENTS = [
    "cycles", "instructions", "branch-misses",
    "L1-icache-load-misses", "L1-dcache-load-misses",
    "stalled-cycles-frontend", "stalled-cycles-backend",
]

SCALING_TU = os.path.join(SCRIPT_DIR, "routing", "dense_index_scaling.bench.cpp")


def run(cmd, cwd=None, check=True):
    print(f"  $ {' '.join(cmd)}")
    return subprocess.run(cmd, cwd=cwd, check=check, capture_output=True, text=True)


def configure_and_build(build_dir, no_build, defines=None):
    bin_path = os.path.join(build_dir, "crg", "benchmarks", BIN_NAME)
    alt_bin_path = os.path.join(build_dir, "crg", "benchmarks", "Release", BIN_NAME)

    if no_build:
        for p in (bin_path, alt_bin_path):
            if os.path.exists(p):
                return p
        print(f"ERROR: --no-build given but no binary found under {build_dir}", file=sys.stderr)
        sys.exit(1)

    print(f">> Configuring ({build_dir})")
    cmake_args = [
        "cmake", "-S", GRIDZERO_ROOT, "-B", build_dir,
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCRG_BUILD_BENCHMARKS=ON",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    ]
    for define in (defines or []):
        cmake_args.append(f"-D{define}")
    run(cmake_args)

    print(">> Building (Release, Run_CRG_Benchmarks)")
    run(["cmake", "--build", build_dir, "--config", "Release",
         "--target", "Run_CRG_Benchmarks", "--parallel"])

    for p in (bin_path, alt_bin_path):
        if os.path.exists(p):
            return p
    print(f"ERROR: build succeeded but binary not found under {build_dir}", file=sys.stderr)
    sys.exit(1)


def run_benchmarks(bin_path, filter_regex, min_time, repetitions=None):
    with tempfile.NamedTemporaryFile(mode="r", suffix=".json", delete=False) as tf:
        json_path = tf.name
    args = [bin_path, "--benchmark_format=json", f"--benchmark_out={json_path}",
            "--benchmark_time_unit=ns"]
    if filter_regex:
        args.append(f"--benchmark_filter={filter_regex}")
    if min_time:
        args.append(f"--benchmark_min_time={min_time}")
    if repetitions:
        args.append(f"--benchmark_repetitions={repetitions}")
        args.append("--benchmark_report_aggregates_only=true")
    print(">> Running benchmark sweep")
    run(args)
    with open(json_path, "r", encoding="utf-8") as f:
        data = json.load(f)
    os.remove(json_path)
    return data


def collect_machine_metadata(defines=None):
    meta = {
        "hostname": platform.node(),
        "os": f"{platform.system()} {platform.release()}",
        "os_version": platform.version(),
        "machine": platform.machine(),
        "cpu": platform.processor() or "unknown",
        "cpu_count": os.cpu_count(),
        "python": platform.python_version(),
        "timestamp": datetime.datetime.now().isoformat(timespec="seconds"),
        "config": ", ".join(defines) if defines else "default",
    }

    try:
        import psutil  # optional — not part of stdlib
        meta["ram_gb"] = round(psutil.virtual_memory().total / (1024 ** 3), 1)
    except ImportError:
        meta["ram_gb"] = "unknown (install psutil for RAM detection)"

    try:
        if platform.system() == "Windows":
            r = run(["cl"], check=False)
            first_line = (r.stderr or r.stdout).splitlines()[0] if (r.stderr or r.stdout) else "unknown"
            meta["compiler"] = first_line.strip()
        else:
            for cc in ("c++", "g++", "clang++"):
                if shutil.which(cc):
                    r = run([cc, "--version"], check=False)
                    meta["compiler"] = r.stdout.splitlines()[0].strip()
                    break
            else:
                meta["compiler"] = "unknown"
    except Exception:
        meta["compiler"] = "unknown"

    meta["revision"] = "unknown"
    try:
        r = run(["p4", "changes", "-m", "1", f"{GRIDZERO_ROOT}/...#have"], check=False)
        if r.returncode == 0 and r.stdout.strip():
            meta["revision"] = r.stdout.strip().splitlines()[0]
    except FileNotFoundError:
        pass

    return meta


def run_perf_pass(bin_path, min_time):
    """Linux-only. Re-runs each comparison benchmark under `perf stat` and
    parses the counters. Returns {} on any other platform or if perf is
    missing — callers must treat that as 'not available', not an error."""
    if platform.system() != "Linux" or not shutil.which("perf"):
        return {}

    print(">> Running PMU counter pass (perf stat, Linux)")
    names = sorted({n for pair in COMPARISON_PAIRS for n in pair})
    filter_regex = "^(" + "|".join(names) + ")$"
    events = ",".join(PERF_EVENTS)

    cmd = ["perf", "stat", "-e", events, "--",
           bin_path, f"--benchmark_filter={filter_regex}"]
    if min_time:
        cmd.append(f"--benchmark_min_time={min_time}")

    r = subprocess.run(cmd, capture_output=True, text=True)
    # perf stat writes the counter table to stderr.
    counters = {}
    for line in r.stderr.splitlines():
        m = re.match(r"\s*([\d,]+)\s+([a-zA-Z0-9_-]+)", line)
        if m:
            value = int(m.group(1).replace(",", ""))
            counters[m.group(2)] = value

    if "cycles" in counters and "instructions" in counters and counters["cycles"] > 0:
        counters["ipc"] = round(counters["instructions"] / counters["cycles"], 3)

    return counters


def start_powermetrics_capture():
    """macOS-only, best-effort. Launches `powermetrics` in the background so it
    samples P-Cluster/E-Cluster frequency for the duration of the upcoming
    benchmark run. Returns (proc, out_path), or (None, None) on any other
    platform, if the binary is missing, or if not running as root --
    powermetrics requires root and this script never prompts for a password;
    re-run the whole invocation under sudo instead."""
    if platform.system() != "Darwin" or not shutil.which("powermetrics"):
        return None, None
    if os.geteuid() != 0:
        print("NOTE: powermetrics needs root -- re-run with "
              "'sudo python3 bench_runner.py --no-build ...' to capture P/E "
              "cluster frequency data. Skipping for this run.", file=sys.stderr)
        return None, None

    out_path = tempfile.mktemp(suffix=".powermetrics.txt")
    proc = subprocess.Popen(
        ["powermetrics", "--samplers", "cpu_power", "-i", "500", "-o", out_path],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return proc, out_path


def stop_powermetrics_capture(proc, out_path):
    """Stops the background capture started above and reduces the raw sample
    stream to a per-cluster summary. 'freq_drop_pct_peak_to_tail' compares the
    peak sampled frequency to the average of the second half of the run --
    a large drop is thermal throttling under sustained load, not a fixed
    architectural ceiling. Returns {} if nothing was captured."""
    if proc is None:
        return {}
    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()

    if not out_path or not os.path.exists(out_path):
        return {}
    with open(out_path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()
    os.remove(out_path)

    clusters = {}
    for m in re.finditer(r"^\s*(\S+)-Cluster HW active frequency:\s*([\d.]+)\s*MHz", text, re.MULTILINE):
        clusters.setdefault(m.group(1), {}).setdefault("freq_mhz", []).append(float(m.group(2)))
    for m in re.finditer(r"^\s*(\S+)-Cluster HW active residency:\s*([\d.]+)%", text, re.MULTILINE):
        clusters.setdefault(m.group(1), {}).setdefault("residency_pct", []).append(float(m.group(2)))

    summary = {}
    for name, series in clusters.items():
        freqs = series.get("freq_mhz", [])
        if not freqs:
            continue
        second_half = freqs[max(1, len(freqs) // 2):]
        peak = max(freqs)
        tail_avg = sum(second_half) / len(second_half)
        drop_pct = round(100.0 * (peak - tail_avg) / peak, 1) if peak > 0 else 0.0
        residency = series.get("residency_pct", [])
        summary[name] = {
            "samples": len(freqs),
            "freq_mhz_min": round(min(freqs), 0),
            "freq_mhz_max": round(peak, 0),
            "freq_mhz_avg": round(sum(freqs) / len(freqs), 0),
            "residency_pct_avg": round(sum(residency) / len(residency), 1) if residency else None,
            "freq_drop_pct_peak_to_tail": drop_pct,
            "throttling_suspected": drop_pct > 15.0,
        }
    return summary


def run_scaling_probe(build_dir):
    """Times the compile of the DenseIndexStore scaling TU (which declares
    domains at N=8..256 models in increasing order) in isolation, using its
    exact compiler invocation from compile_commands.json. This is the
    'how does it compile as N grows' data point — the runtime ns/call at
    each N already comes out of the normal benchmark JSON."""
    cc_path = os.path.join(build_dir, "compile_commands.json")
    if not os.path.exists(cc_path):
        print("WARNING: no compile_commands.json -- skipping --scaling probe "
              "(re-run without --no-build, or add -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)",
              file=sys.stderr)
        return None

    with open(cc_path, "r") as f:
        entries = json.load(f)

    target = os.path.normcase(os.path.abspath(SCALING_TU))
    entry = next((e for e in entries
                  if os.path.normcase(os.path.abspath(e["file"])) == target), None)
    if entry is None:
        print(f"WARNING: {SCALING_TU} not found in compile_commands.json -- skipping --scaling probe",
              file=sys.stderr)
        return None

    command = entry.get("command") or " ".join(entry.get("arguments", []))
    if not command:
        return None

    import shlex
    import time as _time
    argv = shlex.split(command, posix=(platform.system() != "Windows"))

    # Redirect the object output to a throwaway path so repeated runs don't
    # clobber the real build's .obj/.o.
    tmp_obj = tempfile.mktemp(suffix=".obj" if platform.system() == "Windows" else ".o")
    argv = _retarget_output(argv, tmp_obj)

    print(">> Timing DenseIndexStore scaling TU in isolation")
    t0 = _time.perf_counter()
    r = subprocess.run(argv, cwd=entry.get("directory", build_dir), capture_output=True, text=True)
    elapsed = _time.perf_counter() - t0

    if r.returncode != 0:
        print("WARNING: isolated scaling-TU compile failed:\n" + r.stderr, file=sys.stderr)
        return None

    obj_size = os.path.getsize(tmp_obj) if os.path.exists(tmp_obj) else None
    if os.path.exists(tmp_obj):
        os.remove(tmp_obj)

    return {
        "file": os.path.relpath(SCALING_TU, GRIDZERO_ROOT),
        "compile_seconds": round(elapsed, 2),
        "object_bytes": obj_size,
        "note": "single TU declaring domains at N=8,16,32,64,128,256 models",
    }


def _retarget_output(argv, new_out):
    out = list(argv)
    for i, tok in enumerate(out):
        if tok in ("-o", "/Fo") and i + 1 < len(out):
            out[i + 1] = new_out
            return out
        if tok.startswith("/Fo"):
            out[i] = "/Fo" + new_out
            return out
        if tok.startswith("-o") and len(tok) > 2:
            out[i] = "-o" + new_out
            return out
    # No -o found (unlikely) — just append one.
    out += ["-o", new_out]
    return out


def ns_per_call(b):
    """cpu_time is always present and, per time_unit, is already in the unit
    google-benchmark reports (we force ns via --benchmark_time_unit=ns at the
    call site). The custom 'ns_per_call' UserCounter some *.bench.cpp files
    add is expressed in raw seconds despite its name — don't trust its scale."""
    unit = b.get("time_unit", "ns")
    t = b.get("cpu_time")
    if not isinstance(t, (int, float)):
        return None
    scale = {"ns": 1.0, "us": 1e3, "ms": 1e6, "s": 1e9}.get(unit, 1.0)
    return t * scale


def dram_tail_bps(benchmarks, base_name):
    """Among benchmarks named `base_name/<N>`, return bytes_per_second for the
    largest N — the DRAM-saturated point the sweep was pushed to. Returns None
    if this benchmark family wasn't part of this run (e.g. --filter excluded it)."""
    best_n, best_bps = None, None
    prefix = base_name + "/"
    for b in benchmarks:
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


def render_markdown(meta, bench_json, perf_counters, powermetrics, scaling, filter_regex):
    lines = []
    lines.append(f"# CRG Benchmark Report — {meta['hostname']}")
    lines.append("")
    lines.append("## Machine")
    lines.append("")
    lines.append("| Field | Value |")
    lines.append("|---|---|")
    for k in ("hostname", "os", "os_version", "machine", "cpu", "cpu_count",
              "ram_gb", "compiler", "revision", "config", "timestamp"):
        lines.append(f"| {k} | {meta.get(k, 'n/a')} |")
    lines.append("")

    lines.append("## Benchmark results")
    lines.append("")
    if filter_regex:
        lines.append(f"_Filter applied: `{filter_regex}`_")
        lines.append("")
    lines.append("| Benchmark | ns/call | items/sec |")
    lines.append("|---|---|---|")
    for b in bench_json.get("benchmarks", []):
        name = b.get("name", "?")
        ns = ns_per_call(b)
        ips = b.get("items_per_second")
        ips_str = f"{ips/1e6:.1f}M/s" if isinstance(ips, (int, float)) else "n/a"
        ns_str = f"{ns:.3f}" if isinstance(ns, (int, float)) else "n/a"
        lines.append(f"| {name} | {ns_str} | {ips_str} |")
    lines.append("")

    lines.append("## Hardware ceiling — utilization")
    lines.append("")
    lines.append("_DRAM-bound tail (largest swept N). Utilization = CRG Resolve / Read Stream — "
                  "the single-threaded, read-only ceiling matching CRG Resolve's access pattern. "
                  "memcpy and STREAM Triad are secondary reference bandwidths, not the ceiling "
                  "used for the ratio._")
    lines.append("")
    benchmarks = bench_json.get("benchmarks", [])
    read_stream = dram_tail_bps(benchmarks, "BM_MemCeiling_ReadStream")
    crg_bw      = dram_tail_bps(benchmarks, CEILING_CRG_BENCHMARK)
    memcpy_bw   = dram_tail_bps(benchmarks, "BM_MemCeiling_Memcpy")
    triad_bw    = dram_tail_bps(benchmarks, "BM_MemCeiling_Triad")
    lines.append("| Metric | Value |")
    lines.append("|---|---|")
    lines.append(f"| Read Stream (headline ceiling) | {_gib(read_stream)} |")
    lines.append(f"| CRG Resolve | {_gib(crg_bw)} |")
    if isinstance(read_stream, (int, float)) and read_stream > 0 and isinstance(crg_bw, (int, float)):
        util_str = f"{100.0 * crg_bw / read_stream:.1f}%"
    else:
        util_str = "n/a"
    lines.append(f"| **Utilization (CRG / Read Stream)** | {util_str} |")
    lines.append(f"| memcpy (reference) | {_gib(memcpy_bw)} |")
    lines.append(f"| STREAM Triad (reference) | {_gib(triad_bw)} |")
    lines.append("")

    lines.append("## PMU counters")
    lines.append("")
    if perf_counters:
        lines.append("_Aggregate over all filtered benchmarks in one `perf stat` pass "
                      "(Linux only)._")
        lines.append("")
        lines.append("| Counter | Value |")
        lines.append("|---|---|")
        for k, v in perf_counters.items():
            lines.append(f"| {k} | {v} |")
    else:
        lines.append("PMU: n/a (non-Linux machine, or `perf` not installed).")
    lines.append("")

    lines.append("## Thermal / P-E cluster (macOS powermetrics)")
    lines.append("")
    if powermetrics:
        lines.append("_Captured concurrently with the benchmark run above "
                      "(`powermetrics --samplers cpu_power`, 500 ms interval). "
                      "A large freq drop peak->tail is thermal throttling under "
                      "sustained load, not a fixed architectural ceiling._")
        lines.append("")
        lines.append("| Cluster | Samples | Freq min (MHz) | Freq max (MHz) | Freq avg (MHz) | Residency avg (%) | Freq drop peak->tail | Throttling suspected |")
        lines.append("|---|---|---|---|---|---|---|---|")
        for name, s in sorted(powermetrics.items()):
            lines.append(f"| {name} | {s['samples']} | {s['freq_mhz_min']} | {s['freq_mhz_max']} | "
                          f"{s['freq_mhz_avg']} | {s.get('residency_pct_avg', 'n/a')} | "
                          f"{s['freq_drop_pct_peak_to_tail']}% | {'YES' if s['throttling_suspected'] else 'no'} |")
    else:
        lines.append("n/a (non-macOS machine, `powermetrics` not found, or not run with `sudo`).")
    lines.append("")

    lines.append("## Compile-time scaling probe")
    lines.append("")
    if scaling:
        lines.append(f"- File: `{scaling['file']}`")
        lines.append(f"- Note: {scaling['note']}")
        lines.append(f"- Isolated compile time: **{scaling['compile_seconds']}s**")
        if scaling.get("object_bytes"):
            lines.append(f"- Object size: **{scaling['object_bytes'] / 1024:.1f} KB**")
    else:
        lines.append("Not run (pass `--scaling` to enable).")
    lines.append("")

    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build-dir", default=os.path.join(GRIDZERO_ROOT, "..", "build_bench_runner"))
    ap.add_argument("--no-build", action="store_true", help="reuse an existing build dir/binary")
    ap.add_argument("--filter", default=None, help="google-benchmark --benchmark_filter regex")
    ap.add_argument("--min-time", default=None, help="google-benchmark --benchmark_min_time value, e.g. 1.0s")
    ap.add_argument("--repetitions", default=None, type=int,
                     help="google-benchmark --benchmark_repetitions; auto-enables --benchmark_report_aggregates_only=true")
    ap.add_argument("--scaling", action="store_true", help="also time the DenseIndexStore scaling TU in isolation")
    ap.add_argument("--out-dir", default="results/", help="where to write bench_<host>_<ts>.md/.json")
    ap.add_argument("--define", action="append", default=[], metavar="KEY=VAL",
                     help="extra -DKEY=VAL forwarded to the cmake configure step (repeatable), "
                          "e.g. --define CRG_PLUGINS_ENABLED=1")
    args = ap.parse_args()

    build_dir = os.path.abspath(args.build_dir)
    bin_path = configure_and_build(build_dir, args.no_build, args.define)

    pm_proc, pm_out = start_powermetrics_capture()
    bench_json = run_benchmarks(bin_path, args.filter, args.min_time, args.repetitions)
    powermetrics = stop_powermetrics_capture(pm_proc, pm_out)

    meta = collect_machine_metadata(args.define)
    perf_counters = run_perf_pass(bin_path, args.min_time)
    scaling = run_scaling_probe(build_dir) if args.scaling else None

    stamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    base = f"bench_{meta['hostname']}_{stamp}"
    os.makedirs(args.out_dir, exist_ok=True)
    md_path = os.path.join(args.out_dir, base + ".md")
    json_path = os.path.join(args.out_dir, base + ".json")

    sidecar = {
        "machine": meta,
        "filter": args.filter,
        "benchmarks": bench_json.get("benchmarks", []),
        "perf_counters": perf_counters,
        "powermetrics": powermetrics,
        "scaling_probe": scaling,
    }

    with open(json_path, "w", encoding="utf-8") as f:
        json.dump(sidecar, f, indent=2)
    with open(md_path, "w", encoding="utf-8") as f:
        f.write(render_markdown(meta, bench_json, perf_counters, powermetrics, scaling, args.filter))

    print(f"\nOK -> {md_path}")
    print(f"OK -> {json_path}")
    
    # Auto-trigger aggregation if a specific output directory was provided
    if args.out_dir != ".":
        agg_script = os.path.join(SCRIPT_DIR, "bench_aggregate.py")
        if os.path.exists(agg_script):
            print(f"\n>> Auto-triggering aggregation in {args.out_dir}...")
            agg_out = os.path.join(args.out_dir, "benchmarks_aggregate.md")
            subprocess.run([sys.executable, agg_script, "--dir", args.out_dir, "--out", agg_out])
        else:
            print("\nSend both files back for aggregation (bench_aggregate.py not found locally).")
    else:
        print("\nSend both files back for aggregation (bench_aggregate.py).")


if __name__ == "__main__":
    main()
