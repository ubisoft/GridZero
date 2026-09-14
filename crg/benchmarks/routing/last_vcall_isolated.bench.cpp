// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG MICRO-BENCHMARK — Isolated last-vcall: call rax vs call [vptr]
// =============================================================================
//
// Why this benchmark exists
// -------------------------
// dod_vs_oop.bench.cpp's *_FullChain benchmarks compare Find()+fnptr-call
// (CRG stack) against Find()+Dispatch-wrapper+vcall (OOP stack). That is a
// full-architecture comparison, not a single-instruction isolation:
//   - Find() is the SAME generic CapabilityRouter<Domain>::Find<Contract>
//     template on both sides — identical cost, not a confound, but also not
//     what "kill the last vcall" is about.
//   - Both sides are monomorphic (one bound target each) with no cache
//     eviction, which per btb_stability.bench.cpp puts both calls on the
//     "BTB always hits" plateau — the one place where mechanism differences,
//     if any, are hardest to see, not easiest.
// Its FullChain tie (DOD ~6.8ns, OOP ~6.65ns) is real and explained by the
// above, but it must not be read as "isolated dispatch-instruction cost."
//
// What THIS benchmark measures
// -----------------------------
// Only the terminal call, nothing else:
//   Variant A: call through a plain function pointer  -> "call rax"
//   Variant B: call through a base-class vtable pointer -> "call [vptr]"
// No CapabilityRouter, no Find(), no Dispatch() wrapper, no lambda. A single
// warm object (no eviction — residency is covered separately by
// cold_data_dispatch.bench.cpp), a single bound target (monomorphic — target
// diversity is covered separately by btb_stability.bench.cpp). The pointer
// (fn ptr or base ptr) is stored `volatile` so the compiler must reload it
// from memory and issue a genuine indirect call every iteration instead of
// inlining/devirtualizing to a direct call.
//
// This isolates the ONE remaining structural difference between the two
// mechanisms in the best case for both: a vtable call requires one extra
// memory read (object -> vptr) before the indirect call than a fn-ptr call
// does (pointer already in hand -> indirect call). Expected: small and
// possibly within noise — if the CRG narrative is right, the real tax lives
// in residency and predictability (the other two benchmarks), not here.
// =============================================================================

#include <benchmark/benchmark.h>
#include <cstdint>

namespace crg::bench::lastcall
{
    struct Params { std::uint64_t x; };
    using FnPtr = void (*)(Params&);

#if defined(_MSC_VER)
#define LASTCALL_NOINLINE __declspec(noinline)
#else
#define LASTCALL_NOINLINE __attribute__((noinline))
#endif

    LASTCALL_NOINLINE static void FnTarget(Params& p) {
        benchmark::DoNotOptimize(p.x);
    }

    struct IBase {
        virtual void Execute(Params&) const = 0;
        virtual ~IBase() = default;
    };

    struct Impl final : IBase {
        LASTCALL_NOINLINE void Execute(Params& p) const override {
            benchmark::DoNotOptimize(p.x);
        }
    };

#undef LASTCALL_NOINLINE

} // namespace crg::bench::lastcall

// =============================================================================
// Variant A — call rax (plain function pointer)
// =============================================================================
static void BM_LastCall_FnPtr(benchmark::State& state) {
    using namespace crg::bench::lastcall;

    static FnPtr volatile s_fn = &FnTarget;
    Params p{ 1 };

    for (auto _ : state) {
        FnPtr f = s_fn;   // forced reload — cannot be constant-propagated
        f(p);
        benchmark::DoNotOptimize(p);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_LastCall_FnPtr);

// =============================================================================
// Variant B — call [vptr] (virtual dispatch through a base pointer)
// =============================================================================
static void BM_LastCall_VCall(benchmark::State& state) {
    using namespace crg::bench::lastcall;

    static const Impl s_impl;
    static IBase* volatile s_iface = const_cast<Impl*>(&s_impl);
    Params p{ 1 };

    for (auto _ : state) {
        IBase* i = s_iface;   // forced reload — cannot be devirtualized
        i->Execute(p);
        benchmark::DoNotOptimize(p);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_LastCall_VCall);
