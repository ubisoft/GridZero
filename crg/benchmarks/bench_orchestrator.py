#!/usr/bin/env python3

# Copyright (c) Ubisoft. All Rights Reserved.
# Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

# =============================================================================
# CRG MICRO-BENCHMARK ORCHESTRATOR (CROSS-PLATFORM)
# =============================================================================

import os
import sys
import json
import argparse
import subprocess
import glob
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from bench_runner import collect_machine_metadata, start_powermetrics_capture, stop_powermetrics_capture

# OS-agnostic dynamic paths
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "../.."))
BUILD_DIR = os.path.join(REPO_ROOT, "build")

# OS detection for the CMake executable extension
EXE_EXT = ".exe" if os.name == "nt" else ""
BENCH_BIN = os.path.join(BUILD_DIR, "crg", "benchmarks", f"Run_CRG_Benchmarks{EXE_EXT}")

def run_cmd(cmd, cwd=None):
    """Runs a command and exits if it fails."""
    print(f">> Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        print(f"[FAIL] Error: Command failed with code {result.returncode}")
        sys.exit(result.returncode)

def main():
    parser = argparse.ArgumentParser(description="CRG Benchmark Orchestrator")
    parser.add_argument("--filter", help="Benchmark filter (e.g. BM_Router)", default="")
    parser.add_argument("--out-dir", help="Output directory for the reports", default="results/")
    parser.add_argument("--repetitions", default=None, type=int,
                         help="google-benchmark --benchmark_repetitions; auto-enables --benchmark_report_aggregates_only=true")
    parser.add_argument("--define", action="append", default=[], metavar="KEY=VAL",
                         help="extra -DKEY=VAL forwarded to the cmake configure step (repeatable), "
                              "e.g. --define CRG_PLUGINS_ENABLED=1")
    args = parser.parse_args()

    print("=======================================================")
    print("       CRG: CROSS-PLATFORM BENCHMARK ORCHESTRATOR      ")
    print("=======================================================\n")

    # 1. CMake configure and build - always reconfigured if --define is passed,
    #    otherwise only if the binary doesn't exist yet.
    cmake_args = ["-DCMAKE_BUILD_TYPE=Release", "-DCRG_BUILD_BENCHMARKS=ON", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"]
    for define in args.define:
        cmake_args.append(f"-D{define}")

    if args.define or not os.path.exists(BENCH_BIN):
        print(">> Configuring project with CMake...")
        run_cmd(["cmake", "-S", REPO_ROOT, "-B", BUILD_DIR] + cmake_args)
        print("\n>> Building target Run_CRG_Benchmarks (Release)...")
        run_cmd(["cmake", "--build", BUILD_DIR, "--config", "Release", "--target", "Run_CRG_Benchmarks", "--parallel"])

    # 2. Runtime benchmark execution
    meta = collect_machine_metadata(args.define)
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    base = f"bench_{meta['hostname']}_{timestamp}"
    cmd_bench = [BENCH_BIN, "--benchmark_counters_tabular=true"]

    if args.filter:
        cmd_bench.append(f"--benchmark_filter={args.filter}")
    if args.repetitions:
        cmd_bench.append(f"--benchmark_repetitions={args.repetitions}")
        cmd_bench.append("--benchmark_report_aggregates_only=true")

    raw_json_out = None
    if args.out_dir:
        os.makedirs(args.out_dir, exist_ok=True)
        raw_json_out = os.path.join(args.out_dir, f"{base}.raw.json")
        cmd_bench.extend(["--benchmark_format=json", f"--benchmark_out={raw_json_out}"])

    print("\n>> Executing Runtime Benchmarks...")
    pm_proc, pm_out = start_powermetrics_capture()
    run_cmd(cmd_bench)
    powermetrics = stop_powermetrics_capture(pm_proc, pm_out)

    # 2b. Wrap google-benchmark's raw JSON in the same "sidecar" format
    #     as bench_runner.py ({"machine": {...}, "benchmarks": [...]})
    #     so bench_aggregate.py can identify the machine and config.
    if raw_json_out and os.path.exists(raw_json_out):
        with open(raw_json_out, "r", encoding="utf-8") as f:
            raw_bench = json.load(f)
        sidecar = {
            "machine": meta,
            "filter": args.filter or None,
            "benchmarks": raw_bench.get("benchmarks", []),
            "powermetrics": powermetrics,
        }
        json_out = os.path.join(args.out_dir, f"{base}.json")
        with open(json_out, "w", encoding="utf-8") as f:
            json.dump(sidecar, f, indent=2)
        os.remove(raw_json_out)
        print(f">> Wrapped sidecar written to: {json_out}")

    # 3. Compile-time scaling run
    scaling_script = os.path.join(SCRIPT_DIR, "bench_compile_scaling.py")
    if os.path.exists(scaling_script):
        print("\n=======================================================")
        print("       CRG: COMPILER SCALING PROBE MATRIX              ")
        print("=======================================================\n")
        run_cmd([sys.executable, scaling_script])
        
        scaling_report_src = os.path.join(SCRIPT_DIR, "scaling_report.md")
        if args.out_dir and os.path.exists(scaling_report_src):
            scaling_report_dst = os.path.join(args.out_dir, f"scaling_report_{timestamp}.md")
            os.replace(scaling_report_src, scaling_report_dst)
            print(f">> Scaling report saved to: {scaling_report_dst}")

    # 4. Auto-aggregation (if an output directory is specified)
    if args.out_dir:
        agg_script = os.path.join(SCRIPT_DIR, "bench_aggregate.py")
        if os.path.exists(agg_script):
            print("\n>> Auto-triggering aggregation...")
            md_out = os.path.join(args.out_dir, "benchmarks_aggregate.md")
            run_cmd([sys.executable, agg_script, "--dir", args.out_dir, "--out", md_out])
            
            # Injecting the scaling report into the global markdown
            scaling_reports = glob.glob(os.path.join(args.out_dir, "scaling_report*.md"))
            if scaling_reports:
                print(f">> Injecting {len(scaling_reports)} Scaling Reports into {md_out}...")
                with open(md_out, "a", encoding="utf-8") as f_out:
                    f_out.write("\n\n## Compile-Time & Binary-Size Scaling\n\n")
                    f_out.write(f"> Aggregated from {len(scaling_reports)} scaling test(s) in `{args.out_dir}`\n\n")
                    for rep in scaling_reports:
                        f_out.write(f"### Source: {os.path.basename(rep)}\n\n")
                        with open(rep, "r", encoding="utf-8") as f_in:
                            f_out.write(f_in.read())
                        f_out.write("\n\n")

    print("\n=======================================================")
    print("       [OK] Bench sweep & Scaling complete.              ")
    print("=======================================================")

if __name__ == "__main__":
    main()
