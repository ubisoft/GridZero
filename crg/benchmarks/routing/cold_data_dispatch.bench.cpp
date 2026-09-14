// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG MICRO-BENCHMARK — Cold-data dispatch: CRG fn-ptr vs OOP vptr
// =============================================================================
//
// The Claim (Act IV)
// ------------------
// "The virtual call couples L1I to L1D: the CPU must read the object from
//  memory to resolve the vptr before it can fetch the next instruction."
//
// CRG path: the function pointer lives in the Tensor (stationary, L1 hot).
//           Entity data is read for the payload only — after dispatch resolves.
//           Cache miss on entity data ≠ cache miss on control flow.
//
// OOP path: the vptr is co-located with the object.
//           Cache miss on entity data = cache miss on control flow.
//           L1I stalls waiting for L1D to resolve the vptr.
//
// IMPORTANT — polymorphism, not just indirection
// -----------------------------------------------
// A base-class pointer is not enough to expose the stall: if every entity's
// vptr resolves to the SAME concrete type, the indirect call site is
// monomorphic and the BTB predicts the target with near-100% accuracy from
// history alone (see btb_stability.bench.cpp: 1-target ~2ns, already close
// to a direct call). The CPU speculates past the vptr load entirely, hiding
// exactly the coupling this benchmark exists to measure.
//
// The OOP entity therefore cycles through 4 distinct concrete impl types
// (OOPImpl0..OOPImpl3) — enough to saturate BTB misprediction per
// btb_stability's own measurements (4 targets ≈ 16 targets ≈ 64 targets),
// without inflating N artificially. This makes the vptr genuinely
// data-dependent, so resolving it really does require the cold load.
//
// Setup
// -----
// N entities of 64 B (one cache line each) in a heap array.
// Before each timed iteration, the entity array is evicted from L1/L2 via a
// 2 MiB competing array (larger than L2). The Tensor / fn-ptr is kept warm.
//
// CRG: fn ptr resolved from warm Tensor (separate from entity memory).
// OOP: dispatch via IOOPDispatch* (base class pointer, 4 concrete types
//      cycling per entity — prevents both devirt and BTB masking).
//      vptr co-located with entity data — loaded from the same cold line.
//
// This benchmark is NOT about entity sorting. It measures a fundamental
// architectural property: any system that co-locates dispatch metadata with
// data incurs an L1I/L1D dependency on every cache miss.
//
// Expected (4 GHz, 32 KiB L1D, 512 KiB L2, 32 MiB L3)
// -------------------------------------------------------
//   N=1024  (~64 KiB, fits L2): mild advantage for CRG
//   N=4096  (~256 KiB, L2 edge): clear CRG lead — vptr miss stalls pipeline
//   N=16384 (~1 MiB, L3):        larger gap
//   N=65536 (~4 MiB, L3 edge):   maximum advantage
//
// A prior revision of this benchmark used a single concrete OOP type and
// measured the OPPOSITE result (OOP faster than CRG at every N) — that was
// an artifact of BTB prediction on a monomorphic call site, not evidence
// against the architecture. See the polymorphism note above.
// =============================================================================

#include <benchmark/benchmark.h>
#include "crg/crg.hpp"
#include "crg/models/model_key.hpp"

#include <cstdint>
#include <new>
#include <vector>

// =============================================================================
// Shared eviction buffer — kept outside bench functions to survive iterations
// =============================================================================
namespace
{
    // 2 MiB > L2 (512 KiB) → flushes entity array from L2 between iterations
    static std::vector<std::uint8_t> s_Eviction(2u * 1024u * 1024u, 0u);

    static void FlushL2() {
        volatile std::uint64_t sum = 0;
        const std::uint64_t* p = reinterpret_cast<const std::uint64_t*>(s_Eviction.data());
        const std::size_t n = s_Eviction.size() / sizeof(std::uint64_t);
        for (std::size_t i = 0; i < n; ++i) sum += p[i];
        benchmark::DoNotOptimize(sum);
    }
} // namespace

// =============================================================================
// CRG DOMAIN
// =============================================================================
namespace crg::bench::cold
{
    struct ColdDomain {};
    struct ColdModel  {};

    struct ColdContract {
        struct Params {
            std::uint64_t  value;
            std::uint64_t* sink;
        };
    };
    static_assert(crg::capabilities::IsStaticContract<ColdContract>);

#if defined(_MSC_VER)
    __declspec(noinline)
#else
    __attribute__((noinline))
#endif
    static void ColdExecute(ColdContract::Params& p) {
        *p.sink += p.value;
        benchmark::DoNotOptimize(*p.sink);
    }

    template<typename TModel>
    struct ColdCap : crg::capabilities::Capability<ColdContract> {
        static void Execute(ColdContract::Params& p) { ColdExecute(p); }
    };

} // namespace crg::bench::cold

CRG_DECLARE_DOMAIN(crg::bench::cold::ColdDomain)
CRG_DEFINE_DOMAIN(crg::bench::cold::ColdDomain)
CRG_DECLARE_DOMAIN_MODELS(crg::bench::cold::ColdDomain, crg::bench::cold::ColdModel)

namespace crg::bench::cold {
    namespace { static const crg::capabilities::CapabilityBinding<ColdDomain, ColdModel, ColdCap> s_cold; }
}

// =============================================================================
// OOP INTERFACE
// =============================================================================
namespace crg::bench::cold
{
    struct IOOPDispatch {
        virtual void Execute(std::uint64_t value, std::uint64_t* sink) const = 0;
        virtual ~IOOPDispatch() = default;
    };

    // 4 distinct concrete types — enough to saturate BTB misprediction
    // (see btb_stability.bench.cpp). Each writes to its own g_OOPSide slot
    // so the linker cannot fold the bodies back into one identical target.
    static std::uint64_t g_OOPSide[4]{};

#if defined(_MSC_VER)
#define COLD_OOP_NOINLINE __declspec(noinline)
#else
#define COLD_OOP_NOINLINE __attribute__((noinline))
#endif

    struct OOPImpl0 : IOOPDispatch {
        COLD_OOP_NOINLINE void Execute(std::uint64_t value, std::uint64_t* sink) const override {
            *sink += value;
            g_OOPSide[0] += value;
            benchmark::DoNotOptimize(*sink);
            benchmark::DoNotOptimize(g_OOPSide[0]);
        }
    };
    struct OOPImpl1 : IOOPDispatch {
        COLD_OOP_NOINLINE void Execute(std::uint64_t value, std::uint64_t* sink) const override {
            *sink += value;
            g_OOPSide[1] += value;
            benchmark::DoNotOptimize(*sink);
            benchmark::DoNotOptimize(g_OOPSide[1]);
        }
    };
    struct OOPImpl2 : IOOPDispatch {
        COLD_OOP_NOINLINE void Execute(std::uint64_t value, std::uint64_t* sink) const override {
            *sink += value;
            g_OOPSide[2] += value;
            benchmark::DoNotOptimize(*sink);
            benchmark::DoNotOptimize(g_OOPSide[2]);
        }
    };
    struct OOPImpl3 : IOOPDispatch {
        COLD_OOP_NOINLINE void Execute(std::uint64_t value, std::uint64_t* sink) const override {
            *sink += value;
            g_OOPSide[3] += value;
            benchmark::DoNotOptimize(*sink);
            benchmark::DoNotOptimize(g_OOPSide[3]);
        }
    };

#undef COLD_OOP_NOINLINE

} // namespace crg::bench::cold

// =============================================================================
// Entity layouts — both exactly 64 B (one cache line)
// =============================================================================
namespace
{
    using namespace crg::bench::cold;

    // CRG entity: aligned to cache line. fn ptr lives in the Tensor — NOT here.
    struct alignas(64) CRGEntity {
        crg::models::ModelToken<ColdDomain> token;
        std::uint64_t payload;
    };

    // OOP entity: aligned to cache line. vptr (via base ptr) co-located with data.
    // iface is IOOPDispatch* → base-class ptr forces real virtual dispatch.
    // implStorage holds ONE of 4 possible concrete types (placement-constructed
    // per entity) so the vptr value genuinely varies — see the polymorphism
    // note at the top of this file.
    union OOPImplStorage {
        OOPImpl0 t0; OOPImpl1 t1; OOPImpl2 t2; OOPImpl3 t3;
        OOPImplStorage() {}
        ~OOPImplStorage() {}
    };

    struct alignas(64) OOPEntity {
        OOPImplStorage implStorage; // vptr is the first word of whichever member is active
        IOOPDispatch* iface;       // base-class ptr — prevents devirtualization
        std::uint64_t  payload;
    };

} // namespace

// =============================================================================
// CRG BENCHMARK — fn ptr from warm Tensor, entity data evicted before each pass
// =============================================================================
static void BM_ColdData_CRG(benchmark::State& state) {
    using namespace crg::bench::cold;

    const std::size_t N = static_cast<std::size_t>(state.range(0));
    auto token = crg::models::ModelToken<ColdDomain>::FromType<ColdModel>();

    std::vector<CRGEntity> entities(N);
    for (std::size_t i = 0; i < N; ++i) {
        entities[i].token   = token;
        entities[i].payload = static_cast<std::uint64_t>(i + 1);
    }

    // Warm Tensor once — fn ptr stays in L1 across all iterations
    {
        auto cap = crg::routing::CapabilityRouter<ColdDomain>::Find<ColdContract>(token);
        std::uint64_t warmSink = 0;
        ColdContract::Params p{ 1, &warmSink };
        cap(p);
    }

    std::uint64_t sink = 0;

    for (auto _ : state) {
        // Evict entity array from L2 — Tensor remains warm
        state.PauseTiming();
        FlushL2();
        sink = 0;
        state.ResumeTiming();

        // CRG dispatch: Find() reads from warm Tensor, not from cold entity line
        for (std::size_t i = 0; i < N; ++i) {
            auto cap = crg::routing::CapabilityRouter<ColdDomain>::Find<ColdContract>(entities[i].token);
            ColdContract::Params p{ entities[i].payload, &sink };
            cap(p);
        }
        benchmark::DoNotOptimize(sink);
    }

    const std::int64_t total =
        static_cast<std::int64_t>(state.iterations()) *
        static_cast<std::int64_t>(N);
    state.SetItemsProcessed(total);
    state.SetBytesProcessed(total * static_cast<std::int64_t>(sizeof(CRGEntity)));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(total),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_ColdData_CRG)->RangeMultiplier(4)->Range(1 << 10, 1 << 16);

// =============================================================================
// OOP BENCHMARK — vptr co-located with cold entity data
// =============================================================================
static void BM_ColdData_OOP(benchmark::State& state) {
    using namespace crg::bench::cold;

    const std::size_t N = static_cast<std::size_t>(state.range(0));

    std::vector<OOPEntity> entities(N);
    for (std::size_t i = 0; i < N; ++i) {
        entities[i].payload = static_cast<std::uint64_t>(i + 1);
        // Cycle through 4 concrete types so the vptr is genuinely data-dependent.
        switch (i % 4) {
            case 0: entities[i].iface = new (&entities[i].implStorage.t0) OOPImpl0(); break;
            case 1: entities[i].iface = new (&entities[i].implStorage.t1) OOPImpl1(); break;
            case 2: entities[i].iface = new (&entities[i].implStorage.t2) OOPImpl2(); break;
            default: entities[i].iface = new (&entities[i].implStorage.t3) OOPImpl3(); break;
        }
    }

    std::uint64_t sink = 0;

    for (auto _ : state) {
        // Evict entity array from L2 — vptr evicted along with data
        state.PauseTiming();
        FlushL2();
        sink = 0;
        state.ResumeTiming();

        // OOP dispatch: vptr load IS the same cache miss as data load
        for (std::size_t i = 0; i < N; ++i) {
            entities[i].iface->Execute(entities[i].payload, &sink);
        }
        benchmark::DoNotOptimize(sink);
    }

    const std::int64_t total =
        static_cast<std::int64_t>(state.iterations()) *
        static_cast<std::int64_t>(N);
    state.SetItemsProcessed(total);
    state.SetBytesProcessed(total * static_cast<std::int64_t>(sizeof(OOPEntity)));
    state.counters["ns_per_call"] = benchmark::Counter(
        static_cast<double>(total),
        benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
}
BENCHMARK(BM_ColdData_OOP)->RangeMultiplier(4)->Range(1 << 10, 1 << 16);
