#!/usr/bin/env python3

# Copyright (c) Ubisoft. All Rights Reserved.
# Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

# =============================================================================
# CRG COMPILE-TIME & BINARY-SIZE SCALING BENCHMARK
# =============================================================================

import os
import time
import subprocess

MODELS_COUNTS = [10, 50, 100]
CAPS_COUNTS = [10, 50, 100]

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "../.."))

INCLUDES = [
    f"-I{os.path.join(PROJECT_ROOT, 'crg', 'libs', 'crg_core', 'include')}",
    f"-I{os.path.join(PROJECT_ROOT, 'libs', 'crg_core', 'include')}",
    f"-I{PROJECT_ROOT}",
    "-I."
]

CXX = "clang++"

# FIX HERE: injecting the macros required by crg/core/config.hpp
CXXFLAGS = [
    "-std=c++17",
    "-O3",
    "-DNDEBUG",
    "-DCRG_PLUGINS_ENABLED=0",
    '-DCRG_SHARED_LIB_PREFIX="lib"',
    '-DCRG_SHARED_LIB_EXTENSION=".dylib"'
]

GEN_CPP = os.path.join(SCRIPT_DIR, "generated_scaling.cpp")
GEN_OBJ = os.path.join(SCRIPT_DIR, "generated_scaling.o")
GEN_BIN = os.path.join(SCRIPT_DIR, "generated_scaling.out")
REPORT_PATH = os.path.join(SCRIPT_DIR, "scaling_report.md")

def generate_cpp(models, caps):
    lines = [
        "// AUTO-GENERATED SCALING PROBE",
        "#include \"crg/crg.hpp\"",
        "#include \"crg/models/model_key.hpp\"",
        "#include \"crg/routing/capability_router.hpp\"",
        "",
        "struct BenchDomain {};",
        "CRG_DECLARE_DOMAIN(BenchDomain)",
        "CRG_DEFINE_DOMAIN(BenchDomain)",
        ""
    ]
    
    for m in range(models):
        lines.append(f"struct Model{m} {{}};")
        lines.append(f"CRG_DECLARE_DOMAIN_MODELS(BenchDomain, Model{m})")
    lines.append("")
    
    for c in range(caps):
        lines.append(f"struct Contract{c} {{ struct Params {{ int x; }}; }};")
        lines.append(f"template<typename TModel, typename TAt> struct Cap{c} : crg::capabilities::Capability<Contract{c}> {{")
        lines.append(f"    static void Execute(typename Contract{c}::Params& p) {{ p.x++; }}")
        lines.append("};")
    lines.append("")
    
    for m in range(models):
        for c in range(caps):
            lines.append(f"namespace {{ static const crg::capabilities::CapabilityBinding<BenchDomain, Model{m}, Cap{c}> s_b_{m}_{c}; }}")
            
    lines.append("")
    lines.append("int main() {")
    lines.append("    auto h = crg::models::ModelToken<BenchDomain>::FromType<Model0>();")
    lines.append("    auto cap = crg::routing::CapabilityRouter<BenchDomain>::Find<Contract0>(h);")
    lines.append("    Contract0::Params p{0}; if (cap) cap(p);")
    lines.append("    return p.x;")
    lines.append("}")
    
    with open(GEN_CPP, "w") as f:
        f.write("\n".join(lines))

def measure():
    results = []
    total_runs = len(MODELS_COUNTS) * len(CAPS_COUNTS)
    current = 0
    
    print(f"Dynamic project root resolved to: {PROJECT_ROOT}")
    print(f"Starting matrix ({total_runs} combinations)...")
    
    for m in MODELS_COUNTS:
        for c in CAPS_COUNTS:
            current += 1
            print(f"[{current}/{total_runs}] Compiling {m} models x {c} caps ({m*c} bindings)...", end="", flush=True)
            
            generate_cpp(m, c)
            
            t0 = time.time()
            comp_res = subprocess.run([CXX] + CXXFLAGS + INCLUDES + ["-c", GEN_CPP, "-o", GEN_OBJ], capture_output=True)
            t_comp = time.time() - t0
            
            if comp_res.returncode != 0:
                print(" FAILED (Compile)")
                print(comp_res.stderr.decode())
                break
                
            obj_size = os.path.getsize(GEN_OBJ)
            
            t0 = time.time()
            link_res = subprocess.run([CXX] + CXXFLAGS + [GEN_OBJ, "-o", GEN_BIN], capture_output=True)
            t_link = time.time() - t0
            
            if link_res.returncode != 0:
                print(" FAILED (Link)")
                print(link_res.stderr.decode())
                break
                
            bin_size = os.path.getsize(GEN_BIN)
            
            if os.path.exists(GEN_OBJ): os.remove(GEN_OBJ)
            if os.path.exists(GEN_BIN): os.remove(GEN_BIN)
            
            print(f" {t_comp+t_link:.2f}s")
            
            results.append({
                "models": m, "caps": c, "bindings": m*c,
                "compile_s": t_comp, "link_s": t_link, "total_s": t_comp + t_link,
                "obj_kb": obj_size / 1024.0, "bin_kb": bin_size / 1024.0
            })
            
    if os.path.exists(GEN_CPP):
        os.remove(GEN_CPP)
    return results

def print_markdown_report(results):
    with open(REPORT_PATH, "w") as f:
        f.write("# CRG Compile-Time & Binary-Size Scaling\n\n")
        f.write("| Models | Caps | Bindings | Compile (s) | Link (s) | Total (s) | Obj Size (KB) | Bin Size (KB) |\n")
        f.write("|---|---|---|---|---|---|---|---|\n")
        for r in results:
            f.write(f"| {r['models']} | {r['caps']} | {r['bindings']} | {r['compile_s']:.2f} | {r['link_s']:.2f} | {r['total_s']:.2f} | {r['obj_kb']:.1f} | {r['bin_kb']:.1f} |\n")
    print(f"\n[OK] Report saved to {REPORT_PATH}")

if __name__ == "__main__":
    res = measure()
    if res:
        print_markdown_report(res)
