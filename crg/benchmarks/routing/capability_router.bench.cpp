// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG MICRO-BENCHMARK — CapabilityRouter::Find end-to-end
// =============================================================================
//
// What we measure
// ---------------
// `CapabilityRouter<TDomain>::Find<TContract>(token, args...)` is the public
// entry point used by ModelShell::Invoke and by external feature glue. Its
// runtime cost is the sum of:
//   1. token.IsValid() — 1 load + branch on the underlying dense slot
//   2. Horner offset compute — pure constexpr expansion for Volume==1 (free)
//   3. arena[offset]  — 1 vector::operator[] = 1 base ptr load + indexed load
//   4. cell.Resolve(ctx) — see dispatch_cell.bench.cpp for that floor
// Steps 3+4 dominate. Step 3 introduces ONE indirection that DispatchCell does
// not see (the std::vector header is 24 B and may be in a different cache
// line than the cell payload). The delta between this bench and
// BM_DispatchCell_EmptyFallback at equal N quantifies that indirection.
//
// Cache analysis cookbook
// -----------------------
// sizeof(Cell) ≈ 48 B. The vector backing the arena allocates `N * 48 B` of
// heap; the std::vector header itself (3 ptrs = 24 B) lives in static storage
// because the arena is anchored via UniversalAnchor (monolith mode = static
// local). So:
//   - First Find()    : header load (cold) + cell load (cold) → 2 cache misses.
//   - Subsequent Find(): header is in L1; only the cell load varies with N.
//
// The bench warms the header by calling Find once before the timed loop.
// =============================================================================

#include <benchmark/benchmark.h>

// Master header — required because CRG_DECLARE_DOMAIN expands to symbols that
// reference ::crg::discovery::DomainArenaPopulator, only fully visible via the
// umbrella include chain (crg.hpp → crg_discovery.hpp → arena_populator.hpp).
#include "crg/crg.hpp"
#include "crg/models/model_key.hpp"        // CRG_DECLARE_MODEL

#include <cstddef>
#include <cstdint>

// =============================================================================
// MOCK DOMAIN + CONTRACT + MODEL (DOD branch, 0D space, NullContext)
// =============================================================================
namespace crg::bench::router
{
    struct BenchRouterDomain
    {
    };

    struct BenchRouterContract
    {
        struct Params
        {
            std::uint64_t x;
        };
    };

    struct BenchRouterModel
    {
    };

    static_assert(crg::capabilities::IsStaticContract<BenchRouterContract>,
                  "Router bench must target the DOD branch");

#if defined(_MSC_VER)
    __declspec(noinline) static void NoopExecute(BenchRouterContract::Params& p)
    {
        benchmark::DoNotOptimize(p.x);
    }
#else
    __attribute__((noinline)) static void NoopExecute(BenchRouterContract::Params& p)
    {
        benchmark::DoNotOptimize(p.x);
    }
#endif
} // namespace crg::bench::router

// Domain registration must be at top-level (macro produces template specs).
CRG_DECLARE_DOMAIN(crg::bench::router::BenchRouterDomain)
CRG_DECLARE_DOMAIN_MODELS(crg::bench::router::BenchRouterDomain,
    crg::bench::router::BenchRouterModel)

// =============================================================================
// CAPABILITY REGISTRATION — static binding populates the arena at startup
// =============================================================================
namespace crg::bench::router
{
    template<typename TModel>
    struct BenchCap : crg::capabilities::Capability<BenchRouterContract> {
        static void Execute(BenchRouterContract::Params& p) { NoopExecute(p); }
    };

    namespace { static const crg::capabilities::CapabilityBinding<BenchRouterDomain, BenchRouterModel, BenchCap> s_Binding; }
}

CRG_DEFINE_DOMAIN(crg::bench::router::BenchRouterDomain)

namespace
{
    using namespace crg::bench::router;
    using Cell = crg::routing::DispatchCell<BenchRouterDomain, BenchRouterContract>;

    // Prime: trigger arena construction (lazy init) + warm instruction cache.
    // Arena size = 1 (one model registered). Bench uses slot 0 for all N iterations.
    static void PrimeArena(std::size_t /*count*/)
    {
        auto t = crg::models::ModelToken<BenchRouterDomain>::FromType<BenchRouterModel>();
        auto cap = crg::routing::CapabilityRouter<BenchRouterDomain>::Find<BenchRouterContract>(t);
        benchmark::DoNotOptimize(cap);
    }
} // namespace

// =============================================================================
// BENCH — repeated Find() with the SAME token. The compiler cannot fold the
// call: arena content is heap-allocated and DoNotOptimize forces an observable
// effect on the result. Variation across N exposes the cell-load latency only,
// since token/Horner are loop-invariant.
// =============================================================================
static void BM_CapabilityRouter_FindSameHandle(benchmark::State& state)
{
    const std::size_t count = static_cast<std::size_t>(state.range(0));
    PrimeArena(count);

    // Single model registered: slot 0. Arena always fits in L1.
    // This bench measures the Find() path overhead, not cache scaling.
    auto token = crg::models::ModelToken<BenchRouterDomain>::FromType<BenchRouterModel>();

    for (auto _ : state)
    {
        auto cap =
            crg::routing::CapabilityRouter<BenchRouterDomain>::Find<BenchRouterContract>(token);
        benchmark::DoNotOptimize(cap);
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_CapabilityRouter_FindSameHandle)->RangeMultiplier(8)->Range(1 << 10, 1 << 22);

// =============================================================================
// BENCH — sweep over `count` distinct hashes in a tight loop, each resolved
// through the same public Find(hash)-backed path CapabilityRouter's own
// callers use (crg::models::ModelToken::FromKey). Only one model is actually
// registered (arena size 1), so the vast majority of these resolve to an
// invalid token — this measures repeated Find() cost across varying input,
// not a genuine multi-model arena walk. There is deliberately no way to
// construct a ModelToken from a raw slot outside crg_core (see
// model_token.hpp/model_token.md): a benchmark never gets to reach past the
// public API just to make its own loop cheaper.
// =============================================================================
static void BM_CapabilityRouter_FindArenaSweep(benchmark::State& state)
{
    const std::size_t count = static_cast<std::size_t>(state.range(0));
    PrimeArena(count);

    for (auto _ : state)
    {
        std::uint64_t hits = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            auto token = crg::models::ModelToken<BenchRouterDomain>::FromKey(static_cast<crg::u64>(i));
            auto cap = crg::routing::CapabilityRouter<BenchRouterDomain>
                           ::Find<BenchRouterContract>(token);
            benchmark::DoNotOptimize(cap);
            hits += static_cast<std::uint64_t>(static_cast<bool>(cap));
        }
        benchmark::DoNotOptimize(hits);
        benchmark::ClobberMemory();
    }

    const std::int64_t total = static_cast<std::int64_t>(state.iterations()) *
                               static_cast<std::int64_t>(count);

    state.SetItemsProcessed(total);
    state.SetBytesProcessed(total * static_cast<std::int64_t>(sizeof(Cell)));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(total),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_CapabilityRouter_FindArenaSweep)->RangeMultiplier(8)->Range(1 << 10, 1 << 22);
