// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG MICRO-BENCHMARK — BranchlessDispatchCell vs DispatchCell
// =============================================================================
//
// What we measure
// ---------------
// BranchlessDispatchCell<D,C>::Resolve(ctx) against DispatchCell<D,C>::Resolve(ctx)
// across K = {0, 4, 16, 64} dynamic rules, to answer:
//   "When is BranchlessDispatchCell worth its cache footprint?"
//
// Key facts:
//   sizeof(DispatchCell)           ≈  48 B  (1 std::vector hdr + fp + bool)
//   sizeof(BranchlessDispatchCell) ≈ 1040 B (2 × 64-entry fptr arrays + uint32)
//
// Therefore at N=1K:
//   DispatchCell arena  ≈  48 KB  → fits in L1d (≤ 48 KB on most x86_64)
//   BranchlessCell arena ≈ 1 MB   → spills to L2
//
// Expected crossover:
//   K=0 fallback path : DispatchCell wins (no branching in either, but DC is L1)
//   K large, hot loop : BranchlessCell may tie or win (all conds parallel in SoA)
//
// The benchmark uses Bind() — the public API — instead of writing raw members.
// Each profile is constructed via static helper Capability types so Bind() can
// fire the right compile-time branch (HasConfigType / no HasConfigType).
// =============================================================================

#include <benchmark/benchmark.h>

#include "crg/capabilities/binding_target.hpp"
#include "crg/capabilities/capability_handle.hpp"
#include "crg/capabilities/contract_traits.hpp"
#include "crg/routing/capability_routing_traits.hpp"
#include "crg/routing/tensor_arena.hpp"
#include "crg/routing/branchless_dispatch_cell.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

// =============================================================================
// MOCK DOMAIN + CONTRACT
// =============================================================================
namespace crg::bench::branchless
{
    struct BLDomain {};
    struct BLContract {
        struct Params { std::uint64_t x; };
        struct RuleContext {};
    };

    static_assert(crg::capabilities::IsStaticContract<BLContract>,
                  "Bench must target the DOD branch");

#if defined(_MSC_VER)
    __declspec(noinline) static void NoopExecute(BLContract::Params& p) {
        benchmark::DoNotOptimize(p.x);
    }
#else
    __attribute__((noinline)) static void NoopExecute(BLContract::Params& p) {
        benchmark::DoNotOptimize(p.x);
    }
#endif

} // namespace crg::bench::branchless

// =============================================================================
// BUILD HELPERS
// DispatchCell and BranchlessDispatchCell share the same Bind() interface.
// We fill them with K rules using BindingTarget directly to stay pure.
// =============================================================================
namespace
{
    using namespace crg::bench::branchless;
    using DC  = crg::routing::DispatchCell<BLDomain, BLContract>;
    using BDC = crg::routing::BranchlessDispatchCell<BLDomain, BLContract>;
    using Ctx = crg::routing::ContextTypeOf<BLDomain, BLContract>;

    // Minimal Impl type without ConfigType → HasConfigType<>::Value = false → Bind() fallback path
    // Must derive from Capability<BLContract> so HasConfigType works correctly.
    struct FallbackImpl : crg::capabilities::Capability<BLContract> {
        static void Execute(BLContract::Params& p) { NoopExecute(p); }
    };

    // Impl with ConfigType → HasConfigType<>::Value = true → Bind() dynamic-rule path
    // CondFalse: always-false condition (worst-case scan: all miss → fallback)
    struct CondFalseConfig {
        constexpr bool Condition(const Ctx&) const { return false; }
    };
    struct RuleImplFalse : crg::capabilities::Capability<BLContract, CondFalseConfig> {
        static void Execute(BLContract::Params& p) { NoopExecute(p); }
    };

    // CondTrue: always-true condition (first hit wins)
    struct CondTrueConfig {
        constexpr bool Condition(const Ctx&) const { return true; }
    };
    struct RuleImplTrue : crg::capabilities::Capability<BLContract, CondTrueConfig> {
        static void Execute(BLContract::Params& p) { NoopExecute(p); }
    };

    // Shared BindingTarget factory
    static crg::capabilities::BindingTarget<BLContract> MakeFallbackTarget() {
        crg::capabilities::BindingTarget<BLContract> t;
        t.m_Target = &NoopExecute;
        return t;
    }
    static crg::capabilities::BindingTarget<BLContract> MakeRuleTarget() {
        return MakeFallbackTarget();  // same function pointer, different Impl path
    }

    // ----- DispatchCell arena builders -----

    static void FillDC_Fallback(DC& cell) {
        static const FallbackImpl s_impl{};
        cell.Bind<FallbackImpl>(MakeFallbackTarget(), &s_impl);
    }

    static void FillDC_KRulesFalse(DC& cell, int k) {
        static const RuleImplFalse s_ruleImpl{};
        for (int i = 0; i < k; ++i)
            cell.Bind<RuleImplFalse>(MakeRuleTarget(), &s_ruleImpl);
        FillDC_Fallback(cell);
    }

    static void FillDC_KRulesFirstTrue(DC& cell, int k) {
        static const RuleImplTrue  s_trueImpl{};
        static const RuleImplFalse s_falseImpl{};
        cell.Bind<RuleImplTrue>(MakeRuleTarget(), &s_trueImpl);
        for (int i = 1; i < k; ++i)
            cell.Bind<RuleImplFalse>(MakeRuleTarget(), &s_falseImpl);
        FillDC_Fallback(cell);
    }

    // ----- BranchlessDispatchCell arena builders -----

    static void FillBDC_Fallback(BDC& cell) {
        static const FallbackImpl s_impl{};
        cell.Bind<FallbackImpl>(MakeFallbackTarget(), &s_impl);
    }

    static void FillBDC_KRulesFalse(BDC& cell, int k) {
        const int capped = (k > static_cast<int>(BDC::MaxRules)) ? BDC::MaxRules : k;
        static const RuleImplFalse s_ruleImpl{};
        for (int i = 0; i < capped; ++i)
            cell.Bind<RuleImplFalse>(MakeRuleTarget(), &s_ruleImpl);
        FillBDC_Fallback(cell);
    }

    static void FillBDC_KRulesFirstTrue(BDC& cell, int k) {
        static const RuleImplTrue  s_trueImpl{};
        static const RuleImplFalse s_falseImpl{};
        cell.Bind<RuleImplTrue>(MakeRuleTarget(), &s_trueImpl);
        const int capped = (k > static_cast<int>(BDC::MaxRules)) ? BDC::MaxRules : k;
        for (int i = 1; i < capped; ++i)
            cell.Bind<RuleImplFalse>(MakeRuleTarget(), &s_falseImpl);
        FillBDC_Fallback(cell);
    }

    // ----- Inner sweep helpers -----

    template<typename TCell>
    static inline void RunCellSweep(benchmark::State& state,
                                    std::vector<TCell>& arena,
                                    std::size_t count)
    {
        Ctx ctx{};

        // Warm-up
        for (auto& cell : arena)
            benchmark::DoNotOptimize(cell.Resolve(ctx));

        for (auto _ : state) {
            std::uint64_t hits = 0;
            for (std::size_t i = 0; i < count; ++i) {
                auto h = arena[i].Resolve(ctx);
                benchmark::DoNotOptimize(h);
                hits += static_cast<std::uint64_t>(static_cast<bool>(h));
            }
            benchmark::DoNotOptimize(hits);
            benchmark::ClobberMemory();
        }

        const std::int64_t total =
            static_cast<std::int64_t>(state.iterations()) *
            static_cast<std::int64_t>(count);

        state.SetItemsProcessed(total);
        state.SetBytesProcessed(total * static_cast<std::int64_t>(sizeof(TCell)));

        state.counters["ns_per_call"] = benchmark::Counter(
            static_cast<double>(total),
            benchmark::Counter::kIsRate | benchmark::Counter::kInvert);

        state.counters["cell_bytes"] = benchmark::Counter(
            static_cast<double>(sizeof(TCell)),
            benchmark::Counter::kAvgIterations);
    }

} // namespace

// =============================================================================
// DispatchCell cases — K = 0 (fallback only), K = 4 all-miss, K = 4 first-hit,
//                      K = 16 all-miss, K = 64 all-miss
// =============================================================================

static void BM_DC_Fallback(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::vector<DC> arena(N);
    for (auto& c : arena) FillDC_Fallback(c);
    RunCellSweep(state, arena, N);
}
BENCHMARK(BM_DC_Fallback)->RangeMultiplier(8)->Range(1 << 10, 1 << 22);

static void BM_DC_4RulesAllMiss(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::vector<DC> arena(N);
    for (auto& c : arena) FillDC_KRulesFalse(c, 4);
    RunCellSweep(state, arena, N);
}
BENCHMARK(BM_DC_4RulesAllMiss)->RangeMultiplier(8)->Range(1 << 10, 1 << 22);

static void BM_DC_4RulesFirstHit(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::vector<DC> arena(N);
    for (auto& c : arena) FillDC_KRulesFirstTrue(c, 4);
    RunCellSweep(state, arena, N);
}
BENCHMARK(BM_DC_4RulesFirstHit)->RangeMultiplier(8)->Range(1 << 10, 1 << 22);

static void BM_DC_16RulesAllMiss(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::vector<DC> arena(N);
    for (auto& c : arena) FillDC_KRulesFalse(c, 16);
    RunCellSweep(state, arena, N);
}
BENCHMARK(BM_DC_16RulesAllMiss)->RangeMultiplier(8)->Range(1 << 10, 1 << 22);

static void BM_DC_64RulesAllMiss(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::vector<DC> arena(N);
    for (auto& c : arena) FillDC_KRulesFalse(c, 64);
    RunCellSweep(state, arena, N);
}
BENCHMARK(BM_DC_64RulesAllMiss)->RangeMultiplier(8)->Range(1 << 10, 1 << 22);

// =============================================================================
// BranchlessDispatchCell cases — same K set
// =============================================================================

static void BM_BDC_Fallback(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::vector<BDC> arena(N);
    for (auto& c : arena) FillBDC_Fallback(c);
    RunCellSweep(state, arena, N);
}
BENCHMARK(BM_BDC_Fallback)->RangeMultiplier(8)->Range(1 << 10, 1 << 22);

static void BM_BDC_4RulesAllMiss(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::vector<BDC> arena(N);
    for (auto& c : arena) FillBDC_KRulesFalse(c, 4);
    RunCellSweep(state, arena, N);
}
BENCHMARK(BM_BDC_4RulesAllMiss)->RangeMultiplier(8)->Range(1 << 10, 1 << 22);

static void BM_BDC_4RulesFirstHit(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::vector<BDC> arena(N);
    for (auto& c : arena) FillBDC_KRulesFirstTrue(c, 4);
    RunCellSweep(state, arena, N);
}
BENCHMARK(BM_BDC_4RulesFirstHit)->RangeMultiplier(8)->Range(1 << 10, 1 << 22);

static void BM_BDC_16RulesAllMiss(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::vector<BDC> arena(N);
    for (auto& c : arena) FillBDC_KRulesFalse(c, 16);
    RunCellSweep(state, arena, N);
}
BENCHMARK(BM_BDC_16RulesAllMiss)->RangeMultiplier(8)->Range(1 << 10, 1 << 22);

static void BM_BDC_64RulesAllMiss(benchmark::State& state) {
    const std::size_t N = static_cast<std::size_t>(state.range(0));
    std::vector<BDC> arena(N);
    for (auto& c : arena) FillBDC_KRulesFalse(c, 64);
    RunCellSweep(state, arena, N);
}
BENCHMARK(BM_BDC_64RulesAllMiss)->RangeMultiplier(8)->Range(1 << 10, 1 << 22);
