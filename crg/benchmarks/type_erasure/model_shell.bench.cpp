// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG MICRO-BENCHMARK — ModelShell Fixed-Size Buffer Rule-of-5
// =============================================================================
//
// What we measure
// ---------------
// `ModelShell<TDomain>` is a 64-byte cache-line-aligned wrapper that copies
// payloads in-place (no heap). Every transfer across an ECS boundary, an
// arrow RPC frame, or a Mass Transfer DLL handoff goes through one of these
// operations:
//   - Set<TModel>(model)  : placement-new of ModelWrapper<TModel>
//   - copy ctor / op=     : vcall Concept::Clone(buffer)
//   - move ctor / op=     : vcall Concept::Relocate(buffer)
//
// The Concept vtable is the SOLE virtual dispatch in ModelShell. It runs
// during transfer, NOT on the per-tick Invoke hot path — the 1.5 ns charter
// is therefore *not* enforced here. Instead we want to validate that:
//   - Set is dominated by the placement-new of the payload (≤ 5 ns target),
//   - Copy stays within ~10 ns (vcall + memcpy of 56 B),
//   - Move is strictly cheaper than Copy (no payload deep-clone needed).
//
// Cache analysis cookbook
// -----------------------
// sizeof(ModelShell) == CRG_MODEL_SHELL_SIZE == 64 B (one cache line). An
// arena of N shells is exactly N cache lines. Sweep should show:
//   N=1K     ~64 KB   → on the L1/L2 boundary.
//   N=10K    ~640 KB  → spills to L2/L3.
//   N=1M     ~64 MB   → DRAM-bound.
// Any non-monotonic ledge in ns_per_op signals a vtable cache miss
// (Concept's vptr lives in .rodata; cold runs pay the icache cost too).
// =============================================================================

#include <benchmark/benchmark.h>

// Master header — required because CRG_DECLARE_DOMAIN expands to symbols that
// reference ::crg::discovery::DomainArenaPopulator, only fully visible via the
// umbrella include chain (crg.hpp → crg_discovery.hpp → arena_populator.hpp).
#include "crg/crg.hpp"
#include "crg/models/model_key.hpp"        // CRG_DECLARE_MODEL

#include <cstdint>
#include <vector>

// =============================================================================
// MOCK DOMAIN + MODEL — payload sized to fit comfortably in the fixed-size buffer.
// StorageSize = CRG_MODEL_SHELL_SIZE - sizeof(Concept*) == 56 B on x64.
// ModelWrapper<T> = vptr (8 B) + T. So sizeof(T) <= 48 B is the contract (no heap
// fallback: exceeding it is a compile error via static_assert, not a runtime alloc).
// We pick 5 × u64 == 40 B for a comfortable margin and predictable layout.
// =============================================================================
namespace crg::bench::shell
{
    struct BenchShellDomain
    {
    };

    struct BenchPayload
    {
        std::uint64_t a, b, c, d, e;
    };
} // namespace crg::bench::shell

CRG_DECLARE_DOMAIN(crg::bench::shell::BenchShellDomain)
CRG_DECLARE_DOMAIN_MODELS(crg::bench::shell::BenchShellDomain,
    crg::bench::shell::BenchPayload)

// =============================================================================
// HELPERS
// =============================================================================
namespace
{
    using Shell = crg::shell::ModelShell<crg::bench::shell::BenchShellDomain>;

    static_assert(sizeof(Shell) == CRG_MODEL_SHELL_SIZE,
                  "ModelShell size diverged from cache-line — re-tune the bench.");

    static crg::bench::shell::BenchPayload MakePayload(std::uint64_t seed)
    {
        return { seed, seed + 1, seed + 2, seed + 3, seed + 4 };
    }
} // namespace

// =============================================================================
// BENCH 1 — Set on an empty shell (placement-new path, no prior dtor)
// =============================================================================
static void BM_ModelShell_SetEmpty(benchmark::State& state)
{
    const std::size_t count = static_cast<std::size_t>(state.range(0));
    std::vector<Shell> arena(count);

    for (auto _ : state)
    {
        for (std::size_t i = 0; i < count; ++i)
        {
            arena[i] = Shell{};                        // reset shell
            arena[i].Set(MakePayload(i));              // placement-new
            benchmark::DoNotOptimize(arena[i]);
        }
        benchmark::ClobberMemory();
    }

    const std::int64_t total = static_cast<std::int64_t>(state.iterations()) *
                               static_cast<std::int64_t>(count);
    state.SetItemsProcessed(total);
    state.counters["ns_per_op"] = benchmark::Counter(
        static_cast<double>(total),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_ModelShell_SetEmpty)->RangeMultiplier(8)->Range(1 << 10, 1 << 20);

// =============================================================================
// BENCH 2 — Copy ctor: source arena pre-filled, target arena reset each iter
// Measures Concept::Clone (vcall) + 56 B in-place copy of ModelWrapper.
// =============================================================================
static void BM_ModelShell_Copy(benchmark::State& state)
{
    const std::size_t count = static_cast<std::size_t>(state.range(0));

    std::vector<Shell> src(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        src[i].Set(MakePayload(i));
    }

    for (auto _ : state)
    {
        std::vector<Shell> dst;
        dst.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            dst.emplace_back(src[i]);                  // copy ctor → vcall Clone
            benchmark::DoNotOptimize(dst.back());
        }
        benchmark::ClobberMemory();
    }

    const std::int64_t total = static_cast<std::int64_t>(state.iterations()) *
                               static_cast<std::int64_t>(count);
    state.SetItemsProcessed(total);
    state.counters["ns_per_op"] = benchmark::Counter(
        static_cast<double>(total),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_ModelShell_Copy)->RangeMultiplier(8)->Range(1 << 10, 1 << 20);

// =============================================================================
// BENCH 3 — Move ctor: source arena rebuilt each iter (move drains source).
// Measures Concept::Relocate (vcall) + 56 B move of ModelWrapper.
// =============================================================================
static void BM_ModelShell_Move(benchmark::State& state)
{
    const std::size_t count = static_cast<std::size_t>(state.range(0));

    for (auto _ : state)
    {
        // Rebuilt inside the loop because move drains the source. We do NOT
        // PauseTiming() — the Set cost is uniform across iterations and the
        // counter divides by total ops, so the comparative ratio between
        // Copy and Move remains meaningful. The user reads Move - Set delta
        // to isolate the Relocate vcall + memcpy alone.
        std::vector<Shell> src(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            src[i].Set(MakePayload(i));
        }

        std::vector<Shell> dst;
        dst.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            dst.emplace_back(std::move(src[i]));       // move ctor → vcall Relocate
            benchmark::DoNotOptimize(dst.back());
        }
        benchmark::ClobberMemory();
    }

    // Note: 2 ops per index (Set + Move). The counter normalizes per "op"
    // so the absolute Move cost is (ns_per_op × 2) − (ns_per_op of SetEmpty).
    const std::int64_t total = static_cast<std::int64_t>(state.iterations()) *
                               static_cast<std::int64_t>(count) * 2;
    state.SetItemsProcessed(total);
    state.counters["ns_per_op"] = benchmark::Counter(
        static_cast<double>(total),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_ModelShell_Move)->RangeMultiplier(8)->Range(1 << 10, 1 << 20);
