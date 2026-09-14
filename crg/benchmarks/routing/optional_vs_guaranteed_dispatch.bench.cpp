// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG MICRO-BENCHMARK — Optional-contract branch vs guaranteed-contract call
// =============================================================================
//
// Why this benchmark exists
// --------------------------
// last_vcall_isolated.bench.cpp showed call rax ~= call [vptr] (~0 difference)
// when both sides are reduced to *just* the terminal call. But the two real
// code paths in this codebase are not actually symmetric at the call site:
//
//   Brain / interface path (today):
//       auto* cap = Find<IMove>(typeKey);
//       if (cap) { cap->Execute(params); }      // optional contract
//
//   Muscle / DoD path (today):
//       auto fn = GetTarget(); // CRG_ASSERT(fn), compiles away in Release
//       fn(params);                              // guaranteed contract, no branch
//
// So the historical comparison was quietly conflating two independent
// variables:
//   1. dispatch MECHANISM   -> call rax vs call [vptr]     (measured: ~0)
//   2. dispatch OPTIONALITY -> guaranteed call vs if-guarded call
//
// This benchmark isolates variable 2, crossed with variable 1, so all four
// combinations are directly comparable:
//
//   BM_Guaranteed_FnPtr   -- fn ptr,  no branch   (DoD today)
//   BM_Guaranteed_VCall   -- vtable,  no branch   (mechanism-only baseline)
//   BM_Optional_FnPtr     -- fn ptr,  if-guarded  (branch-only baseline)
//   BM_Optional_VCall     -- vtable,  if-guarded  (Brain today)
//
// The pointer being checked is ALWAYS valid (non-null) in this benchmark,
// matching steady-state execution: the interface exists at runtime, the
// check is defensive, not a genuine 50/50 branch. This is deliberate --
// the real Dispatch() null-check is taken every single time in practice, so
// a fair test asks "does a highly-predictable, always-taken branch cost
// anything," not "what if we made it unpredictable."
//
// If BM_Optional_* ties with BM_Guaranteed_* for the same mechanism, the
// optionality of the contract is not a source of overhead either, and the
// case for "guaranteed contracts are why DoD is fast" collapses alongside
// "fnptr vs vcall is why DoD is fast." Both would point the same way: the
// performance story lives in data residency (cold_data_dispatch.bench.cpp),
// not in dispatch form.
// =============================================================================

#include <benchmark/benchmark.h>
#include <cstdint>

namespace crg::bench::optdispatch
{
    struct Params { std::uint64_t x; };
    using FnPtr = void (*)(Params&);

#if defined(_MSC_VER)
#define OPTDISPATCH_NOINLINE __declspec(noinline)
#else
#define OPTDISPATCH_NOINLINE __attribute__((noinline))
#endif

    OPTDISPATCH_NOINLINE static void FnTarget(Params& p) {
        benchmark::DoNotOptimize(p.x);
    }

    struct IBase {
        virtual void Execute(Params&) const = 0;
        virtual ~IBase() = default;
    };

    struct Impl final : IBase {
        OPTDISPATCH_NOINLINE void Execute(Params& p) const override {
            benchmark::DoNotOptimize(p.x);
        }
    };

#undef OPTDISPATCH_NOINLINE

} // namespace crg::bench::optdispatch

// =============================================================================
// Guaranteed contract — no branch (today's DoD/Muscle shape)
// =============================================================================
static void BM_Guaranteed_FnPtr(benchmark::State& state) {
    using namespace crg::bench::optdispatch;

    static FnPtr volatile s_fn = &FnTarget;
    Params p{ 1 };

    for (auto _ : state) {
        FnPtr f = s_fn;
        f(p);
        benchmark::DoNotOptimize(p);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_Guaranteed_FnPtr);

static void BM_Guaranteed_VCall(benchmark::State& state) {
    using namespace crg::bench::optdispatch;

    static const Impl s_impl;
    static IBase* volatile s_iface = const_cast<Impl*>(&s_impl);
    Params p{ 1 };

    for (auto _ : state) {
        IBase* i = s_iface;
        i->Execute(p);
        benchmark::DoNotOptimize(p);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_Guaranteed_VCall);

// =============================================================================
// Optional contract — if-guarded (today's Brain/interface shape)
// Pointer is always valid; the branch is always taken. Isolates the cost of
// the check itself under the realistic, highly-predictable case.
// =============================================================================
static void BM_Optional_FnPtr(benchmark::State& state) {
    using namespace crg::bench::optdispatch;

    static FnPtr volatile s_fn = &FnTarget;
    Params p{ 1 };

    for (auto _ : state) {
        FnPtr f = s_fn;
        if (f) {
            f(p);
        }
        benchmark::DoNotOptimize(p);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_Optional_FnPtr);

static void BM_Optional_VCall(benchmark::State& state) {
    using namespace crg::bench::optdispatch;

    static const Impl s_impl;
    static IBase* volatile s_iface = const_cast<Impl*>(&s_impl);
    Params p{ 1 };

    for (auto _ : state) {
        IBase* i = s_iface;
        if (i) {
            i->Execute(p);
        }
        benchmark::DoNotOptimize(p);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_Optional_VCall);
