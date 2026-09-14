// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// dispatch_cell.bench.cpp — DispatchCell::Resolve over a contiguous arena, HasDynamicRules true/false

#include <benchmark/benchmark.h>

#include "crg/capabilities/binding_target.hpp"
#include "crg/capabilities/capability_handle.hpp"
#include "crg/capabilities/contract_traits.hpp"
#include "crg/routing/capability_routing_traits.hpp"
#include "crg/routing/tensor_arena.hpp"

#include "../bench_hardware.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace crg::bench::routing
{
    struct BenchDomain
    {
    };

    struct BenchContract
    {
        struct Params
        {
            std::uint64_t x;
        };
        struct RuleContext
        {
        };
    };

    struct BenchContractStatic
    {
        struct Params
        {
            std::uint64_t x;
        };
    };

    static_assert(crg::capabilities::IsStaticContract<BenchContract>,
                  "Bench must target the DOD branch of DispatchCell");

#if defined(_MSC_VER)
    __declspec(noinline) static void NoopExecute(BenchContract::Params& p)
    {
        benchmark::DoNotOptimize(p.x);
    }
    __declspec(noinline) static void NoopExecuteStatic(BenchContractStatic::Params& p)
    {
        benchmark::DoNotOptimize(p.x);
    }
#else
    __attribute__((noinline)) static void NoopExecute(BenchContract::Params& p)
    {
        benchmark::DoNotOptimize(p.x);
    }
    __attribute__((noinline)) static void NoopExecuteStatic(BenchContractStatic::Params& p)
    {
        benchmark::DoNotOptimize(p.x);
    }
#endif

    static bool PredicateFalse(const void*, const BenchContract::RuleContext&)
    {
        return false;
    }

    static bool PredicateTrue(const void*, const BenchContract::RuleContext&)
    {
        return true;
    }
} // namespace crg::bench::routing

namespace
{
    using StaticCell  = crg::routing::DispatchCell<crg::bench::routing::BenchDomain,
                                                    crg::bench::routing::BenchContractStatic>;
    using DynamicCell = crg::routing::DispatchCell<crg::bench::routing::BenchDomain,
                                                    crg::bench::routing::BenchContract>;
    using DynamicRule = crg::routing::DynamicRule<crg::bench::routing::BenchDomain,
                                                  crg::bench::routing::BenchContract>;

    static_assert(sizeof(StaticCell) < sizeof(DynamicCell),
                  "HasDynamicRules = false must shrink the cell");

    enum class Profile
    {
        EmptyNoFallback,
        EmptyWithFallback,
        FourRulesAllMiss,
        FourRulesFirstHit,
    };

    static void FillCell(StaticCell& cell, Profile profile)
    {
        using namespace crg::bench::routing;

        switch (profile)
        {
        case Profile::EmptyNoFallback:
            break;

        case Profile::EmptyWithFallback:
        case Profile::FourRulesAllMiss:
        case Profile::FourRulesFirstHit:
            cell.m_Fallback.m_Target = &NoopExecuteStatic;
            break;
        }
    }

    static void FillCell(DynamicCell& cell, Profile profile)
    {
        using namespace crg::bench::routing;

        switch (profile)
        {
        case Profile::EmptyNoFallback:
            break;

        case Profile::EmptyWithFallback:
            cell.m_Fallback.m_Target = &NoopExecute;
            break;

        case Profile::FourRulesAllMiss:
            for (int k = 0; k < 4; ++k)
            {
                DynamicRule rule{};
                rule.m_Descriptor.m_Target = &NoopExecute;
                rule.m_Predicate           = &PredicateFalse;
                cell.m_DynamicRules.push_back(rule);
            }
            cell.m_Fallback.m_Target = &NoopExecute;
            break;

        case Profile::FourRulesFirstHit:
            for (int k = 0; k < 4; ++k)
            {
                DynamicRule rule{};
                rule.m_Descriptor.m_Target = &NoopExecute;
                rule.m_Predicate           = (k == 0) ? &PredicateTrue : &PredicateFalse;
                cell.m_DynamicRules.push_back(rule);
            }
            cell.m_Fallback.m_Target = &NoopExecute;
            break;
        }
    }

    template<typename TCell>
    static std::vector<TCell> MakeArena(std::size_t count, Profile profile)
    {
        std::vector<TCell> arena(count);
        for (auto& cell : arena)
        {
            FillCell(cell, profile);
        }
        return arena;
    }

    template<typename TCell>
    static void WarmArena(std::vector<TCell>& arena, const typename TCell::ContextType& ctx)
    {
        for (auto& cell : arena)
        {
            benchmark::DoNotOptimize(cell.Resolve(ctx));
        }
    }

    template<typename TCell, typename TParams>
    static inline void RunArenaSweepGuaranteed(benchmark::State& state, Profile profile)
    {
        const std::size_t count = static_cast<std::size_t>(state.range(0));
        auto              arena = MakeArena<TCell>(count, profile);
        typename TCell::ContextType ctx{};
        WarmArena(arena, ctx);

        for (auto _ : state)
        {
            TParams p{};
            for (std::size_t i = 0; i < count; ++i)
            {
                auto handle = arena[i].Resolve(ctx);
                handle(p);
            }
            benchmark::ClobberMemory();
        }

        const std::int64_t total = static_cast<std::int64_t>(state.iterations()) *
                                   static_cast<std::int64_t>(count);

        state.SetItemsProcessed(total);
        state.SetBytesProcessed(total * static_cast<std::int64_t>(sizeof(TCell)));

        state.counters["ns_per_call"] =
            benchmark::Counter(static_cast<double>(total),
                               benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
    }

    template<typename TCell>
    static inline void RunArenaSweepOptional(benchmark::State& state, Profile profile)
    {
        const std::size_t count = static_cast<std::size_t>(state.range(0));
        auto              arena = MakeArena<TCell>(count, profile);
        typename TCell::ContextType ctx{};
        WarmArena(arena, ctx);

        for (auto _ : state)
        {
            std::uint64_t hits = 0;
            for (std::size_t i = 0; i < count; ++i)
            {
                auto handle = arena[i].Resolve(ctx);
                benchmark::DoNotOptimize(handle);
                hits += static_cast<std::uint64_t>(static_cast<bool>(handle));
            }
            benchmark::DoNotOptimize(hits);
            benchmark::ClobberMemory();
        }

        const std::int64_t total = static_cast<std::int64_t>(state.iterations()) *
                                   static_cast<std::int64_t>(count);

        state.SetItemsProcessed(total);
        state.SetBytesProcessed(total * static_cast<std::int64_t>(sizeof(TCell)));

        state.counters["ns_per_call"] =
            benchmark::Counter(static_cast<double>(total),
                               benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
    }
} // namespace

namespace
{
    constexpr std::size_t DispatchRangeMinBytes = std::size_t{1} << 16;
    constexpr std::size_t DispatchRangeMaxBytes = std::size_t{1} << 28;
}

#define CRG_BENCH_DISPATCH_RANGE(TCell) \
    RangeMultiplier(8)->Range(DispatchRangeMinBytes / sizeof(TCell), \
                               DispatchRangeMaxBytes / sizeof(TCell))

static void BM_DispatchCell_EmptyNoFallback_Static(benchmark::State& state)
{
    RunArenaSweepOptional<StaticCell>(state, Profile::EmptyNoFallback);
}
BENCHMARK(BM_DispatchCell_EmptyNoFallback_Static)->CRG_BENCH_DISPATCH_RANGE(StaticCell);

static void BM_DispatchCell_EmptyNoFallback_Dynamic(benchmark::State& state)
{
    RunArenaSweepOptional<DynamicCell>(state, Profile::EmptyNoFallback);
}
BENCHMARK(BM_DispatchCell_EmptyNoFallback_Dynamic)->CRG_BENCH_DISPATCH_RANGE(DynamicCell);

static void BM_DispatchCell_EmptyFallback_Static(benchmark::State& state)
{
    RunArenaSweepGuaranteed<StaticCell, crg::bench::routing::BenchContractStatic::Params>(state, Profile::EmptyWithFallback);
}
BENCHMARK(BM_DispatchCell_EmptyFallback_Static)->CRG_BENCH_DISPATCH_RANGE(StaticCell);

static void BM_DispatchCell_EmptyFallback_Dynamic(benchmark::State& state)
{
    RunArenaSweepGuaranteed<DynamicCell, crg::bench::routing::BenchContract::Params>(state, Profile::EmptyWithFallback);
}
BENCHMARK(BM_DispatchCell_EmptyFallback_Dynamic)->CRG_BENCH_DISPATCH_RANGE(DynamicCell);

static void BM_DispatchCell_EmptyFallback_ThreadScaling_Static(benchmark::State& state)
{
    RunArenaSweepGuaranteed<StaticCell, crg::bench::routing::BenchContractStatic::Params>(state, Profile::EmptyWithFallback);
}
BENCHMARK(BM_DispatchCell_EmptyFallback_ThreadScaling_Static)
    ->Arg(DispatchRangeMaxBytes / sizeof(StaticCell))
    ->DenseThreadRange(1, crg::bench::HardwareThreadCeiling(), 1);

static void BM_DispatchCell_EmptyFallback_ThreadScaling_Dynamic(benchmark::State& state)
{
    RunArenaSweepGuaranteed<DynamicCell, crg::bench::routing::BenchContract::Params>(state, Profile::EmptyWithFallback);
}
BENCHMARK(BM_DispatchCell_EmptyFallback_ThreadScaling_Dynamic)
    ->Arg(DispatchRangeMaxBytes / sizeof(DynamicCell))
    ->DenseThreadRange(1, crg::bench::HardwareThreadCeiling(), 1);

static void BM_DispatchCell_FourRulesAllMiss(benchmark::State& state)
{
    RunArenaSweepGuaranteed<DynamicCell, crg::bench::routing::BenchContract::Params>(state, Profile::FourRulesAllMiss);
}
BENCHMARK(BM_DispatchCell_FourRulesAllMiss)->CRG_BENCH_DISPATCH_RANGE(DynamicCell);

static void BM_DispatchCell_FourRulesFirstHit(benchmark::State& state)
{
    RunArenaSweepGuaranteed<DynamicCell, crg::bench::routing::BenchContract::Params>(state, Profile::FourRulesFirstHit);
}
BENCHMARK(BM_DispatchCell_FourRulesFirstHit)->CRG_BENCH_DISPATCH_RANGE(DynamicCell);
