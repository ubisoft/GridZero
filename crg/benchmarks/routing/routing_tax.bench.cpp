// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG MICRO-BENCHMARK — Routing tax: per-component breakdown
// =============================================================================
//
// What we measure
// ---------------
// The routing tax decomposes into five separable costs:
//
//   A) ModelToken::FromType<T>()    — DenseIndexStore linear scan (cold: O(N_models))
//   B) CapabilityRouter::Find()     — arena ptr load + Horner + cell.Resolve()
//   C) CapabilityHandle invocation  — one indirect call via function pointer
//   D) Find + Invoke, token cached  — recurring cost when FromType is paid once
//                                      at token acquisition, not per frame
//   E) Full chain: FromType + Find + Invoke, every call
//
// "Steady state" = token already resolved, same slot every iteration.
// "Cold" = first call; measured separately via DoNotOptimize + iteration guard.
//
// D vs E matters: ModelShell::Invoke() (crg/type_erasure/model_shell.hpp)
// calls Concept::GetToken(), which re-derives the token via FromType<T>()
// on EVERY Invoke() call — it does not cache. So callers going through
// ModelShell pay E every call. Callers holding a raw ModelToken themselves
// (the stage_02 Muscle pattern) pay A once at acquisition and D per frame.
//
// This answers the Q&A question: "What exactly is the 1.5 ns?"
// =============================================================================

#include <benchmark/benchmark.h>
#include "crg/crg.hpp"
#include "crg/models/model_key.hpp"

#include <cstdint>

// =============================================================================
// DOMAIN + MODEL + CONTRACT
// =============================================================================
namespace crg::bench::tax
{
    struct TaxDomain {};
    struct TaxModel  {};

    struct TaxContract {
        struct Params { std::uint64_t x; };
    };

    static_assert(crg::capabilities::IsStaticContract<TaxContract>);

#if defined(_MSC_VER)
    __declspec(noinline) static void NoopExecute(TaxContract::Params& p) {
        benchmark::DoNotOptimize(p.x);
    }
#else
    __attribute__((noinline)) static void NoopExecute(TaxContract::Params& p) {
        benchmark::DoNotOptimize(p.x);
    }
#endif

} // namespace crg::bench::tax

CRG_DECLARE_DOMAIN(crg::bench::tax::TaxDomain)
CRG_DEFINE_DOMAIN(crg::bench::tax::TaxDomain)
CRG_DECLARE_DOMAIN_MODELS(crg::bench::tax::TaxDomain, crg::bench::tax::TaxModel)

// Capability registration: static binding, no dynamic rules
namespace crg::bench::tax
{
    template<typename TModel>
    struct TaxCapability : crg::capabilities::Capability<TaxContract> {
        static void Execute(TaxContract::Params& p) { NoopExecute(p); }
    };

    namespace {
        static const crg::capabilities::CapabilityBinding<TaxDomain, TaxModel, TaxCapability> s_Binding;
    }
} // namespace crg::bench::tax

// =============================================================================
// A) ModelToken::FromType — DenseIndexStore lookup only
//    Warm: the store is already populated, same key every call.
// =============================================================================
static void BM_ModelToken_FromType_Warm(benchmark::State& state) {
    using namespace crg::bench::tax;

    // Prime the store
    auto warmup = crg::models::ModelToken<TaxDomain>::FromType<TaxModel>();
    benchmark::DoNotOptimize(warmup);

    for (auto _ : state) {
        auto h = crg::models::ModelToken<TaxDomain>::FromType<TaxModel>();
        benchmark::DoNotOptimize(h);
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_ModelToken_FromType_Warm);

// =============================================================================
// B) CapabilityRouter::Find — pre-resolved token, steady state
//    Measures: arena base-ptr load + Horner (Volume=1) + cell.Resolve()
// =============================================================================
static void BM_Router_Find_Warm(benchmark::State& state) {
    using namespace crg::bench::tax;

    auto token = crg::models::ModelToken<TaxDomain>::FromType<TaxModel>();

    // Force arena construction
    auto warmup = crg::routing::CapabilityRouter<TaxDomain>::Find<TaxContract>(token);
    benchmark::DoNotOptimize(warmup);

    for (auto _ : state) {
        auto cap = crg::routing::CapabilityRouter<TaxDomain>::Find<TaxContract>(token);
        benchmark::DoNotOptimize(cap);
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_Router_Find_Warm);

// =============================================================================
// C) CapabilityHandle invocation only — function pointer call with Params
//    Isolates the final dispatch step from routing overhead.
// =============================================================================
static void BM_CapabilityHandle_Invoke(benchmark::State& state) {
    using namespace crg::bench::tax;

    auto token = crg::models::ModelToken<TaxDomain>::FromType<TaxModel>();
    auto cap   = crg::routing::CapabilityRouter<TaxDomain>::Find<TaxContract>(token);

    TaxContract::Params p{ 0 };

    for (auto _ : state) {
        cap(p);
        benchmark::DoNotOptimize(p);
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_CapabilityHandle_Invoke);

// =============================================================================
// D) Find + Invoke only — token cached once, NOT re-resolved per call.
//    This is the recurring steady-state cost when the caller holds a
//    long-lived ModelToken (e.g. stored once per entity at spawn time)
//    and only calls Find()+Invoke() every frame. Isolates the tax from
//    FromType's O(N_models) lookup, which the DOD Muscle pattern in
//    stage_02_muscle_0d does NOT pay per-frame — only at token acquisition.
// =============================================================================
static void BM_Router_FindInvoke_Warm(benchmark::State& state) {
    using namespace crg::bench::tax;

    auto token = crg::models::ModelToken<TaxDomain>::FromType<TaxModel>();
    TaxContract::Params p{ 0 };

    // Warm up lazily-initialized statics once
    {
        auto cap = crg::routing::CapabilityRouter<TaxDomain>::Find<TaxContract>(token);
        cap(p);
    }

    for (auto _ : state) {
        auto cap = crg::routing::CapabilityRouter<TaxDomain>::Find<TaxContract>(token);
        cap(p);
        benchmark::DoNotOptimize(p);
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_Router_FindInvoke_Warm);

// =============================================================================
// E) Full chain: FromType + Find + Invoke — the total tax per entity
//    This is what "1.5 ns" refers to in the talk.
// =============================================================================
static void BM_FullChain_FromTypeFindInvoke(benchmark::State& state) {
    using namespace crg::bench::tax;

    // Warm up lazily-initialized statics once
    {
        auto h   = crg::models::ModelToken<TaxDomain>::FromType<TaxModel>();
        auto cap = crg::routing::CapabilityRouter<TaxDomain>::Find<TaxContract>(h);
        TaxContract::Params p{};
        cap(p);
    }

    TaxContract::Params p{ 0 };

    for (auto _ : state) {
        auto h   = crg::models::ModelToken<TaxDomain>::FromType<TaxModel>();
        auto cap = crg::routing::CapabilityRouter<TaxDomain>::Find<TaxContract>(h);
        cap(p);
        benchmark::DoNotOptimize(p);
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_FullChain_FromTypeFindInvoke);
