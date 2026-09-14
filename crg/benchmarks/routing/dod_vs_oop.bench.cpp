// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG MICRO-BENCHMARK — DOD contract vs OOP contract dispatch cost
// =============================================================================
//
// What we measure
// ---------------
// DOD path: CapabilityHandle<TContract> where TContract has `struct Params`.
//           Dispatch = void(*)(Params&) — one indirect function pointer call.
//
// OOP path: CapabilityHandle<IContract> where IContract is polymorphic.
//           Dispatch = virtual operator->() + virtual method call.
//
// Both are measured for:
//   - Routing only: CapabilityRouter::Find()
//   - Invocation only: operator() / Invoke() on a cached handle
//   - Full chain: Find + Invoke
//
// Expected: DOD full chain ≈ 1.5 ns. OOP full chain ≈ 3–5 ns (vptr read + vtable load).
// This directly supports the "DOD path eliminates the vptr stall" claim in Act IV.
// =============================================================================

#include <benchmark/benchmark.h>
#include "crg/crg.hpp"
#include "crg/models/model_key.hpp"

#include <cstdint>

// =============================================================================
// DOD DOMAIN
// =============================================================================
namespace crg::bench::dodoop
{
    struct DODDomain {};
    struct DODModel  {};

    struct DODContract {
        struct Params { std::uint64_t x; };
    };
    static_assert(crg::capabilities::IsStaticContract<DODContract>);

#if defined(_MSC_VER)
    __declspec(noinline) static void DODNoop(DODContract::Params& p) { benchmark::DoNotOptimize(p.x); }
#else
    __attribute__((noinline)) static void DODNoop(DODContract::Params& p) { benchmark::DoNotOptimize(p.x); }
#endif

    template<typename TModel>
    struct DODCap : crg::capabilities::Capability<DODContract> {
        static void Execute(DODContract::Params& p) { DODNoop(p); }
    };

} // namespace crg::bench::dodoop

CRG_DECLARE_DOMAIN(crg::bench::dodoop::DODDomain)
CRG_DEFINE_DOMAIN(crg::bench::dodoop::DODDomain)
CRG_DECLARE_DOMAIN_MODELS(crg::bench::dodoop::DODDomain, crg::bench::dodoop::DODModel)

namespace crg::bench::dodoop {
    namespace { static const crg::capabilities::CapabilityBinding<DODDomain, DODModel, DODCap> s_dod; }
}

// =============================================================================
// OOP DOMAIN
// =============================================================================
namespace crg::bench::dodoop
{
    struct OOPDomain {};
    struct OOPModel  {};

    // Polymorphic contract — NOT a DOD contract (no Params, is polymorphic)
    struct IOOPContract {
        virtual ~IOOPContract() = default;
        virtual void Execute(std::uint64_t& x) const = 0;
    };
    static_assert(!crg::capabilities::IsStaticContract<IOOPContract>);

    template<typename TModel>
    struct OOPCap : crg::capabilities::Capability<IOOPContract> {
#if defined(_MSC_VER)
        __declspec(noinline)
#else
        __attribute__((noinline))
#endif
        void Execute(std::uint64_t& x) const override { benchmark::DoNotOptimize(x); }
    };

} // namespace crg::bench::dodoop

CRG_DECLARE_DOMAIN(crg::bench::dodoop::OOPDomain)
CRG_DEFINE_DOMAIN(crg::bench::dodoop::OOPDomain)
CRG_DECLARE_DOMAIN_MODELS(crg::bench::dodoop::OOPDomain, crg::bench::dodoop::OOPModel)

namespace crg::bench::dodoop {
    namespace { static const crg::capabilities::CapabilityBinding<OOPDomain, OOPModel, OOPCap> s_oop; }
}

// =============================================================================
// DOD BENCHMARKS
// =============================================================================
static void BM_DOD_FindOnly(benchmark::State& state) {
    using namespace crg::bench::dodoop;
    auto token  = crg::models::ModelToken<DODDomain>::FromType<DODModel>();
    auto warmup = crg::routing::CapabilityRouter<DODDomain>::Find<DODContract>(token);
    benchmark::DoNotOptimize(warmup);

    for (auto _ : state) {
        auto cap = crg::routing::CapabilityRouter<DODDomain>::Find<DODContract>(token);
        benchmark::DoNotOptimize(cap);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_DOD_FindOnly);

static void BM_DOD_InvokeOnly(benchmark::State& state) {
    using namespace crg::bench::dodoop;
    auto token = crg::models::ModelToken<DODDomain>::FromType<DODModel>();
    auto cap   = crg::routing::CapabilityRouter<DODDomain>::Find<DODContract>(token);

    DODContract::Params p{};
    for (auto _ : state) {
        cap(p);
        benchmark::DoNotOptimize(p);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_DOD_InvokeOnly);

static void BM_DOD_FullChain(benchmark::State& state) {
    using namespace crg::bench::dodoop;
    // Warm
    { auto h = crg::models::ModelToken<DODDomain>::FromType<DODModel>();
      auto c = crg::routing::CapabilityRouter<DODDomain>::Find<DODContract>(h);
      DODContract::Params p{}; c(p); }

    DODContract::Params p{};
    for (auto _ : state) {
        auto token = crg::models::ModelToken<DODDomain>::FromType<DODModel>();
        auto cap   = crg::routing::CapabilityRouter<DODDomain>::Find<DODContract>(token);
        cap(p);
        benchmark::DoNotOptimize(p);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_DOD_FullChain);

// =============================================================================
// OOP BENCHMARKS
// =============================================================================
static void BM_OOP_FindOnly(benchmark::State& state) {
    using namespace crg::bench::dodoop;
    auto token  = crg::models::ModelToken<OOPDomain>::FromType<OOPModel>();
    auto warmup = crg::routing::CapabilityRouter<OOPDomain>::Find<IOOPContract>(token);
    benchmark::DoNotOptimize(warmup);

    for (auto _ : state) {
        auto cap = crg::routing::CapabilityRouter<OOPDomain>::Find<IOOPContract>(token);
        benchmark::DoNotOptimize(cap);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_OOP_FindOnly);

static void BM_OOP_InvokeOnly(benchmark::State& state) {
    using namespace crg::bench::dodoop;
    auto token = crg::models::ModelToken<OOPDomain>::FromType<OOPModel>();
    auto cap   = crg::routing::CapabilityRouter<OOPDomain>::Find<IOOPContract>(token);

    std::uint64_t x = 0;
    for (auto _ : state) {
        crg::routing::CapabilityRouter<OOPDomain>::Dispatch<IOOPContract>(cap, [&](const IOOPContract& iface) {
            iface.Execute(x);
        });
        benchmark::DoNotOptimize(x);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_OOP_InvokeOnly);

static void BM_OOP_FullChain(benchmark::State& state) {
    using namespace crg::bench::dodoop;
    // Warm
    { auto h = crg::models::ModelToken<OOPDomain>::FromType<OOPModel>();
      auto c = crg::routing::CapabilityRouter<OOPDomain>::Find<IOOPContract>(h); }

    std::uint64_t x = 0;
    for (auto _ : state) {
        auto token = crg::models::ModelToken<OOPDomain>::FromType<OOPModel>();
        auto cap   = crg::routing::CapabilityRouter<OOPDomain>::Find<IOOPContract>(token);
        crg::routing::CapabilityRouter<OOPDomain>::Dispatch<IOOPContract>(cap, [&](const IOOPContract& iface) {
            iface.Execute(x);
        });
        benchmark::DoNotOptimize(x);
    }
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(state.iterations()),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_OOP_FullChain);
