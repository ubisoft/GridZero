// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG MICRO-BENCHMARK — Horner offset cost vs. tensor dimensionality (0D–6D)
// =============================================================================
//
// What we measure
// ---------------
// CapabilitySpace<A, B, C...>::ComputeOffset(coords...) runs inside Find().
// We benchmark the full Find() path with a pre-resolved token at increasing
// dimensionality:
//
//   0D : Volume = 1, offset always 0 (constexpr folded to 0)
//   1D : Volume = 2  (1 multiply-add)
//   2D : Volume = 4  (2 multiply-adds)
//   3D : Volume = 8  (3 multiply-adds)
//   4D : Volume = 16 (4 multiply-adds)
//   5D : Volume = 32 (5 multiply-adds)
//   6D : Volume = 64 (6 multiply-adds)
//
// Each axis has 2 values (enum Ax2). The tensor is small (≤ 64 cells), so it
// always fits in L1. ns_per_call delta between consecutive rows = Horner cost.
//
// NOTE: CRG_DECLARE_DOMAIN_MODELS binds a model to a specific domain via an
// explicit template specialization keyed on the TModel type. The same model
// type CANNOT be registered in multiple domains. Each domain therefore gets
// its own distinct model type (HModel0..HModel6).
// =============================================================================

#include <benchmark/benchmark.h>
#include "crg/crg.hpp"
#include "crg/models/model_key.hpp"

#include <cstdint>

enum class Ax2 : std::uint8_t { V0 = 0, V1 = 1 };
namespace crg { template<> struct EnumTraits<Ax2> { static constexpr std::size_t Count = 2; }; }

namespace crg::bench::horner
{
    struct HD0 {}; struct HD1 {}; struct HD2 {};
    struct HD3 {}; struct HD4 {}; struct HD5 {}; struct HD6 {};

    // Each domain gets its own model type to avoid duplicate explicit specializations.
    struct HModel0 {}; struct HModel1 {}; struct HModel2 {};
    struct HModel3 {}; struct HModel4 {}; struct HModel5 {}; struct HModel6 {};

    struct HContract { struct Params { std::uint64_t x; }; };
    static_assert(crg::capabilities::IsStaticContract<HContract>);

#if defined(_MSC_VER)
    __declspec(noinline) static void NoopExecute(HContract::Params& p) { benchmark::DoNotOptimize(p.x); }
#else
    __attribute__((noinline)) static void NoopExecute(HContract::Params& p) { benchmark::DoNotOptimize(p.x); }
#endif
}

CRG_DECLARE_DOMAIN(crg::bench::horner::HD0) CRG_DEFINE_DOMAIN(crg::bench::horner::HD0)
CRG_DECLARE_DOMAIN(crg::bench::horner::HD1) CRG_DEFINE_DOMAIN(crg::bench::horner::HD1)
CRG_DECLARE_DOMAIN(crg::bench::horner::HD2) CRG_DEFINE_DOMAIN(crg::bench::horner::HD2)
CRG_DECLARE_DOMAIN(crg::bench::horner::HD3) CRG_DEFINE_DOMAIN(crg::bench::horner::HD3)
CRG_DECLARE_DOMAIN(crg::bench::horner::HD4) CRG_DEFINE_DOMAIN(crg::bench::horner::HD4)
CRG_DECLARE_DOMAIN(crg::bench::horner::HD5) CRG_DEFINE_DOMAIN(crg::bench::horner::HD5)
CRG_DECLARE_DOMAIN(crg::bench::horner::HD6) CRG_DEFINE_DOMAIN(crg::bench::horner::HD6)

CRG_DECLARE_DOMAIN_MODELS(crg::bench::horner::HD0, crg::bench::horner::HModel0)
CRG_DECLARE_DOMAIN_MODELS(crg::bench::horner::HD1, crg::bench::horner::HModel1)
CRG_DECLARE_DOMAIN_MODELS(crg::bench::horner::HD2, crg::bench::horner::HModel2)
CRG_DECLARE_DOMAIN_MODELS(crg::bench::horner::HD3, crg::bench::horner::HModel3)
CRG_DECLARE_DOMAIN_MODELS(crg::bench::horner::HD4, crg::bench::horner::HModel4)
CRG_DECLARE_DOMAIN_MODELS(crg::bench::horner::HD5, crg::bench::horner::HModel5)
CRG_DECLARE_DOMAIN_MODELS(crg::bench::horner::HD6, crg::bench::horner::HModel6)

namespace crg::routing {
    using namespace crg::bench::horner;
    template<> struct CapabilityRoutingTraits<HD0, HContract> { using SpaceType = CapabilitySpace<>; };
    template<> struct CapabilityRoutingTraits<HD1, HContract> { using SpaceType = CapabilitySpace<Ax2>; };
    template<> struct CapabilityRoutingTraits<HD2, HContract> { using SpaceType = CapabilitySpace<Ax2, Ax2>; };
    template<> struct CapabilityRoutingTraits<HD3, HContract> { using SpaceType = CapabilitySpace<Ax2, Ax2, Ax2>; };
    template<> struct CapabilityRoutingTraits<HD4, HContract> { using SpaceType = CapabilitySpace<Ax2, Ax2, Ax2, Ax2>; };
    template<> struct CapabilityRoutingTraits<HD5, HContract> { using SpaceType = CapabilitySpace<Ax2, Ax2, Ax2, Ax2, Ax2>; };
    template<> struct CapabilityRoutingTraits<HD6, HContract> { using SpaceType = CapabilitySpace<Ax2, Ax2, Ax2, Ax2, Ax2, Ax2>; };
}

namespace crg::bench::horner {
    // HCap0 (0-D, 1-arg) and HCapN (Dims>0, 2-arg) are the split halves of what
    // was a single cross-dimensional HCap template. Under the pure dimensionality
    // model TAt is inert at Dims==0, so the 0-D baseline point takes the 1-arg
    // form and the swept axes keep the 2-arg form -- identical bodies, no default.
    template<typename TModel>
    struct HCap0 : crg::capabilities::Capability<HContract> {
        static void Execute(HContract::Params& p) { NoopExecute(p); }
    };

    template<typename TModel, typename TAt>
    struct HCapN : crg::capabilities::Capability<HContract> {
        static void Execute(HContract::Params& p) { NoopExecute(p); }
    };

    namespace {
        static const crg::capabilities::CapabilityBinding<HD0, HModel0, HCap0> s_b0;
        static const crg::capabilities::CapabilityBinding<HD1, HModel1, HCapN> s_b1;
        static const crg::capabilities::CapabilityBinding<HD2, HModel2, HCapN> s_b2;
        static const crg::capabilities::CapabilityBinding<HD3, HModel3, HCapN> s_b3;
        static const crg::capabilities::CapabilityBinding<HD4, HModel4, HCapN> s_b4;
        static const crg::capabilities::CapabilityBinding<HD5, HModel5, HCapN> s_b5;
        static const crg::capabilities::CapabilityBinding<HD6, HModel6, HCapN> s_b6;
    }
}

// Each benchmark calls Find() with the all-V0 coordinate corner, arena size = Volume.
// Because Volume ≤ 64 cells × 48 B ≤ 3 KB, the tensor always fits in L1.
// ns_per_call isolates the Horner multiply-add loop and the cell.Resolve() call.
#define HORNER_BENCH(DimTag, ModelTag, NDims, ...)                                                      \
static void BM_Horner_##NDims##D(benchmark::State& state) {                                             \
    using namespace crg::bench::horner;                                                                 \
    auto token  = crg::models::ModelToken<DimTag>::FromType<ModelTag>();                                \
    auto warmup = crg::routing::CapabilityRouter<DimTag>::Find<HContract>(token, ##__VA_ARGS__);        \
    benchmark::DoNotOptimize(warmup);                                                                   \
    for (auto _ : state) {                                                                              \
        auto cap = crg::routing::CapabilityRouter<DimTag>::Find<HContract>(token, ##__VA_ARGS__);       \
        benchmark::DoNotOptimize(cap);                                                                  \
    }                                                                                                   \
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));                             \
    state.counters["dims"] = benchmark::Counter(static_cast<double>(NDims));                            \
    state.counters["ns_per_call"] = benchmark::Counter(                                                 \
        static_cast<double>(state.iterations()),                                                        \
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);                                     \
}                                                                                                       \
BENCHMARK(BM_Horner_##NDims##D)

using namespace crg::bench::horner;
HORNER_BENCH(HD0, HModel0, 0);
HORNER_BENCH(HD1, HModel1, 1, Ax2::V0);
HORNER_BENCH(HD2, HModel2, 2, Ax2::V0, Ax2::V0);
HORNER_BENCH(HD3, HModel3, 3, Ax2::V0, Ax2::V0, Ax2::V0);
HORNER_BENCH(HD4, HModel4, 4, Ax2::V0, Ax2::V0, Ax2::V0, Ax2::V0);
HORNER_BENCH(HD5, HModel5, 5, Ax2::V0, Ax2::V0, Ax2::V0, Ax2::V0, Ax2::V0);
HORNER_BENCH(HD6, HModel6, 6, Ax2::V0, Ax2::V0, Ax2::V0, Ax2::V0, Ax2::V0, Ax2::V0);
