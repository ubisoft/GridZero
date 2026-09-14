// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG MICRO-BENCHMARK — Memory bandwidth ceiling reference
// =============================================================================
//
// What we measure
// ---------------
// `DispatchCell::Resolve` (see routing/dispatch_cell.bench.cpp) is a
// single-threaded, read-only, streaming scan of a contiguous arena. The
// apples-to-apples "hardware ceiling" for that access pattern is a
// single-threaded, read-only DRAM stream — NOT memcpy (adds a write stream)
// and NOT STREAM Triad (2 reads + 1 write). Comparing CRG against memcpy or
// Triad instead of a read-only ceiling would understate the true achievable
// bandwidth for a read-only pattern and make the utilization ratio meaningless.
//
// BM_MemCeiling_ReadStream is therefore the HEADLINE denominator for the
// "CRG reaches the hardware ceiling" utilization ratio:
//     utilization = CRG_Resolve.bytes_per_second / ReadStream.bytes_per_second
//
// BM_MemCeiling_Memcpy and the STREAM kernels (Copy/Scale/Add/Triad) are kept
// as secondary, widely-recognized reference bandwidths — not the ceiling used
// for the ratio.
//
// All benchmarks below are single-threaded, matching CRG Resolve's execution
// model. Range covers 64 KiB -> 256 MiB so the DRAM-bound tail (roughly
// >= 64 MiB) brackets DispatchCell's ~192 MiB DRAM-tail footprint
// (N=4194304 cells * 48 B/cell, see dispatch_cell.bench.cpp).
// =============================================================================

#include <benchmark/benchmark.h>

#include "../bench_hardware.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#define CRG_BENCH_CEILING_RANGE() RangeMultiplier(8)->Range(1 << 16, 1 << 28)

namespace
{
    // -------------------------------------------------------------------------
    // BM_MemCeiling_ReadStream — HEADLINE ceiling
    // -------------------------------------------------------------------------
    // Deliberately trivial: a pure read-only streaming reduction. No branches,
    // no predicates, no filtering, no virtual calls — anything beyond
    // `sum += buffer[i]` would measure something other than raw DRAM read
    // bandwidth and stop being a legitimate ceiling for CRG Resolve.
    static void BM_MemCeiling_ReadStream(benchmark::State& state)
    {
        const std::size_t bytes = static_cast<std::size_t>(state.range(0));
        const std::size_t count = bytes / sizeof(std::uint64_t);
        std::vector<std::uint64_t> buffer(count, 1);

        // Fault every page in before timing so the loop measures steady-state
        // DRAM bandwidth, not first-touch page faults.
        std::uint64_t warm = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            warm += buffer[i];
        }
        benchmark::DoNotOptimize(warm);

        for (auto _ : state)
        {
            std::uint64_t sum = 0;
            for (std::size_t i = 0; i < count; ++i)
            {
                sum += buffer[i];
            }
            benchmark::DoNotOptimize(sum);
        }

        state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                                 static_cast<std::int64_t>(count) *
                                 static_cast<std::int64_t>(sizeof(std::uint64_t)));
    }
    BENCHMARK(BM_MemCeiling_ReadStream)->CRG_BENCH_CEILING_RANGE();

    // -------------------------------------------------------------------------
    // BM_MemCeiling_ReadStream_ThreadScaling — aggregate bandwidth vs thread count
    // -------------------------------------------------------------------------
    // Same read-only stream, fixed at the 256 MiB DRAM-tail footprint, run by
    // N independent threads each over their own private buffer (no sharing,
    // no coherency traffic — this isolates raw memory-controller contention).
    // Tells us whether the single-threaded ceiling above is one core's own
    // limit, or already the whole machine's aggregate DRAM bandwidth.
    static void BM_MemCeiling_ReadStream_ThreadScaling(benchmark::State& state)
    {
        constexpr std::size_t bytes = std::size_t{1} << 28;  // 256 MiB
        const std::size_t     count = bytes / sizeof(std::uint64_t);
        std::vector<std::uint64_t> buffer(count, 1);

        std::uint64_t warm = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            warm += buffer[i];
        }
        benchmark::DoNotOptimize(warm);

        for (auto _ : state)
        {
            std::uint64_t sum = 0;
            for (std::size_t i = 0; i < count; ++i)
            {
                sum += buffer[i];
            }
            benchmark::DoNotOptimize(sum);
        }

        state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                                 static_cast<std::int64_t>(count) *
                                 static_cast<std::int64_t>(sizeof(std::uint64_t)));
    }
    BENCHMARK(BM_MemCeiling_ReadStream_ThreadScaling)
        ->DenseThreadRange(1, crg::bench::HardwareThreadCeiling(), 1);

    // -------------------------------------------------------------------------
    // BM_MemCeiling_Memcpy — secondary reference
    // -------------------------------------------------------------------------
    // SetBytesProcessed uses the conventional "copy size" convention (matches
    // how memcpy bandwidth is normally reported); true DRAM traffic is
    // roughly 2x that (one read stream + one write stream).
    static void BM_MemCeiling_Memcpy(benchmark::State& state)
    {
        const std::size_t bytes = static_cast<std::size_t>(state.range(0));
        std::vector<std::uint8_t> src(bytes, 0x5A);
        std::vector<std::uint8_t> dst(bytes, 0);

        std::memcpy(dst.data(), src.data(), bytes);
        benchmark::DoNotOptimize(dst.data());

        for (auto _ : state)
        {
            std::memcpy(dst.data(), src.data(), bytes);
            benchmark::DoNotOptimize(dst.data());
            benchmark::ClobberMemory();
        }

        state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                                 static_cast<std::int64_t>(bytes));
    }
    BENCHMARK(BM_MemCeiling_Memcpy)->CRG_BENCH_CEILING_RANGE();

    // -------------------------------------------------------------------------
    // STREAM kernels (McCalpin) — secondary reference, the perf-engineering
    // community's standard memory-bandwidth benchmark. `bytes` is the total
    // footprint of the three backing arrays combined (so ReadStream, memcpy
    // and STREAM all reach the DRAM-bound tail at comparable `bytes` values).
    // Byte-processed counts follow the STREAM convention: Copy/Scale touch
    // 2 arrays, Add/Triad touch 3, each element 8 bytes (double).
    // -------------------------------------------------------------------------
    static void FillStreamArrays(std::size_t n, std::vector<double>& a,
                                  std::vector<double>& b, std::vector<double>& c)
    {
        a.assign(n, 0.0);
        b.assign(n, 1.0);
        c.assign(n, 2.0);
    }

    static void BM_MemCeiling_Copy(benchmark::State& state)
    {
        const std::size_t bytes = static_cast<std::size_t>(state.range(0));
        const std::size_t n     = bytes / (3 * sizeof(double));
        std::vector<double> a, b, c;
        FillStreamArrays(n, a, b, c);

        for (auto _ : state)
        {
            for (std::size_t i = 0; i < n; ++i)
            {
                a[i] = b[i];
            }
            benchmark::DoNotOptimize(a.data());
            benchmark::ClobberMemory();
        }

        state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                                 static_cast<std::int64_t>(n) * 2 *
                                 static_cast<std::int64_t>(sizeof(double)));
    }
    BENCHMARK(BM_MemCeiling_Copy)->CRG_BENCH_CEILING_RANGE();

    static void BM_MemCeiling_Scale(benchmark::State& state)
    {
        const std::size_t bytes = static_cast<std::size_t>(state.range(0));
        const std::size_t n     = bytes / (3 * sizeof(double));
        std::vector<double> a, b, c;
        FillStreamArrays(n, a, b, c);
        const double q = 3.0;

        for (auto _ : state)
        {
            for (std::size_t i = 0; i < n; ++i)
            {
                a[i] = q * b[i];
            }
            benchmark::DoNotOptimize(a.data());
            benchmark::ClobberMemory();
        }

        state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                                 static_cast<std::int64_t>(n) * 2 *
                                 static_cast<std::int64_t>(sizeof(double)));
    }
    BENCHMARK(BM_MemCeiling_Scale)->CRG_BENCH_CEILING_RANGE();

    static void BM_MemCeiling_Add(benchmark::State& state)
    {
        const std::size_t bytes = static_cast<std::size_t>(state.range(0));
        const std::size_t n     = bytes / (3 * sizeof(double));
        std::vector<double> a, b, c;
        FillStreamArrays(n, a, b, c);

        for (auto _ : state)
        {
            for (std::size_t i = 0; i < n; ++i)
            {
                a[i] = b[i] + c[i];
            }
            benchmark::DoNotOptimize(a.data());
            benchmark::ClobberMemory();
        }

        state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                                 static_cast<std::int64_t>(n) * 3 *
                                 static_cast<std::int64_t>(sizeof(double)));
    }
    BENCHMARK(BM_MemCeiling_Add)->CRG_BENCH_CEILING_RANGE();

    // BM_MemCeiling_Triad — the STREAM kernel most commonly cited as "the"
    // memory-bandwidth reference by perf engineers.
    static void BM_MemCeiling_Triad(benchmark::State& state)
    {
        const std::size_t bytes = static_cast<std::size_t>(state.range(0));
        const std::size_t n     = bytes / (3 * sizeof(double));
        std::vector<double> a, b, c;
        FillStreamArrays(n, a, b, c);
        const double q = 3.0;

        for (auto _ : state)
        {
            for (std::size_t i = 0; i < n; ++i)
            {
                a[i] = b[i] + q * c[i];
            }
            benchmark::DoNotOptimize(a.data());
            benchmark::ClobberMemory();
        }

        state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                                 static_cast<std::int64_t>(n) * 3 *
                                 static_cast<std::int64_t>(sizeof(double)));
    }
    BENCHMARK(BM_MemCeiling_Triad)->CRG_BENCH_CEILING_RANGE();
} // namespace
