// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG MICRO-BENCHMARKS — main entry point
// =============================================================================
//
// Mirror of crg/tests/core_monolith/main_runner.cpp (Catch2): each *.bench.cpp
// translation unit registers its cases via BENCHMARK(...) and we let
// google/benchmark's BENCHMARK_MAIN expand to int main() here.
//
// Runtime flags worth knowing:
//   --benchmark_min_time=2.0s   : extend per-case sampling for tighter CIs
//   --benchmark_repetitions=5   : aggregate stats (mean / median / stddev)
//   --benchmark_format=json     : machine-readable output for CI gating
//   --benchmark_counters_tabular: align CYC/INS counters in the table
//   --benchmark_filter=...      : run a subset by regex
// =============================================================================

#include <benchmark/benchmark.h>
#include "crg/core/scoped_no_alloc.hpp"
CRG_INSTALL_NO_ALLOC_GUARD()

BENCHMARK_MAIN();
