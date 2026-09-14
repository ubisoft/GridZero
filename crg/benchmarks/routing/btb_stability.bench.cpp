// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG MICRO-BENCHMARK — BTB indirect-call target stability
// =============================================================================
//
// What we measure
// ---------------
// Act IV claims CRG accepts a "localized instruction hiccup" (indirect call).
// The cost of that hiccup depends on BTB prediction. We measure:
//
//   A) 1 target:  single Execute() pointer — BTB always hits
//   B) 4 targets: cycling among 4 distinct Execute() pointers
//   C) 16 targets: BTB prediction degrades on most CPUs
//   D) 64 targets: BTB exhausted — maximum prediction miss overhead
//
// Each arena element holds a resolved CapabilityHandle whose function pointer
// cycles through the target set: entity[i] → kSinks[i % K].
//
// Expected:
//   1 target  : ~1.0–1.5 ns (BTB warm, near-direct-call cost)
//   64 targets: higher cost per call, delta = BTB miss penalty
//
// Q&A ammunition: "When does indirect prediction degrade, and by how much?"
// =============================================================================

#include <benchmark/benchmark.h>
#include "crg/capabilities/binding_target.hpp"
#include "crg/capabilities/capability_handle.hpp"
#include "crg/capabilities/contract_traits.hpp"
#include "crg/routing/capability_routing_traits.hpp"

#include <cstdint>
#include <vector>
#include <array>

namespace crg::bench::btb
{
    struct BTBDomain {};
    struct BTBContract { struct Params { std::uint64_t x; }; };
    static_assert(crg::capabilities::IsStaticContract<BTBContract>);

    static std::uint64_t g_Side[64]{};

} // namespace crg::bench::btb

// 64 distinct noinline sink functions — must be at namespace scope (not inside
// a named namespace block) to avoid MSVC's __attribute__ mis-parse.
// Each is distinct so the linker cannot merge them.
#if defined(_MSC_VER)
#define BTB_SINK(N) \
    __declspec(noinline) static void BtbSink##N(crg::bench::btb::BTBContract::Params& p) { \
        crg::bench::btb::g_Side[(N) % 64] = p.x; \
        benchmark::DoNotOptimize(crg::bench::btb::g_Side[(N) % 64]); \
    }
#else
#define BTB_SINK(N) \
    __attribute__((noinline)) static void BtbSink##N(crg::bench::btb::BTBContract::Params& p) { \
        crg::bench::btb::g_Side[(N) % 64] = p.x; \
        benchmark::DoNotOptimize(crg::bench::btb::g_Side[(N) % 64]); \
    }
#endif

BTB_SINK( 0) BTB_SINK( 1) BTB_SINK( 2) BTB_SINK( 3)
BTB_SINK( 4) BTB_SINK( 5) BTB_SINK( 6) BTB_SINK( 7)
BTB_SINK( 8) BTB_SINK( 9) BTB_SINK(10) BTB_SINK(11)
BTB_SINK(12) BTB_SINK(13) BTB_SINK(14) BTB_SINK(15)
BTB_SINK(16) BTB_SINK(17) BTB_SINK(18) BTB_SINK(19)
BTB_SINK(20) BTB_SINK(21) BTB_SINK(22) BTB_SINK(23)
BTB_SINK(24) BTB_SINK(25) BTB_SINK(26) BTB_SINK(27)
BTB_SINK(28) BTB_SINK(29) BTB_SINK(30) BTB_SINK(31)
BTB_SINK(32) BTB_SINK(33) BTB_SINK(34) BTB_SINK(35)
BTB_SINK(36) BTB_SINK(37) BTB_SINK(38) BTB_SINK(39)
BTB_SINK(40) BTB_SINK(41) BTB_SINK(42) BTB_SINK(43)
BTB_SINK(44) BTB_SINK(45) BTB_SINK(46) BTB_SINK(47)
BTB_SINK(48) BTB_SINK(49) BTB_SINK(50) BTB_SINK(51)
BTB_SINK(52) BTB_SINK(53) BTB_SINK(54) BTB_SINK(55)
BTB_SINK(56) BTB_SINK(57) BTB_SINK(58) BTB_SINK(59)
BTB_SINK(60) BTB_SINK(61) BTB_SINK(62) BTB_SINK(63)

namespace
{
    using namespace crg::bench::btb;
    using ExecFn = void (*)(BTBContract::Params&);
    using Handle = crg::capabilities::CapabilityHandle<BTBContract>;
    using Target = crg::capabilities::BindingTarget<BTBContract>;

    static constexpr std::array<ExecFn, 64> kSinks = {
        BtbSink0,  BtbSink1,  BtbSink2,  BtbSink3,
        BtbSink4,  BtbSink5,  BtbSink6,  BtbSink7,
        BtbSink8,  BtbSink9,  BtbSink10, BtbSink11,
        BtbSink12, BtbSink13, BtbSink14, BtbSink15,
        BtbSink16, BtbSink17, BtbSink18, BtbSink19,
        BtbSink20, BtbSink21, BtbSink22, BtbSink23,
        BtbSink24, BtbSink25, BtbSink26, BtbSink27,
        BtbSink28, BtbSink29, BtbSink30, BtbSink31,
        BtbSink32, BtbSink33, BtbSink34, BtbSink35,
        BtbSink36, BtbSink37, BtbSink38, BtbSink39,
        BtbSink40, BtbSink41, BtbSink42, BtbSink43,
        BtbSink44, BtbSink45, BtbSink46, BtbSink47,
        BtbSink48, BtbSink49, BtbSink50, BtbSink51,
        BtbSink52, BtbSink53, BtbSink54, BtbSink55,
        BtbSink56, BtbSink57, BtbSink58, BtbSink59,
        BtbSink60, BtbSink61, BtbSink62, BtbSink63,
    };

    // Build arena of N handles cycling through K targets
    static std::vector<Handle> MakeHandles(std::size_t N, int K) {
        std::vector<Handle> v(N);
        for (std::size_t i = 0; i < N; ++i) {
            Target t;
            t.m_Target = kSinks[static_cast<std::size_t>(i) % static_cast<std::size_t>(K)];
            Handle h; h = &t; v[i] = h;
        }
        return v;
    }

    static inline void RunBTBBench(benchmark::State& state, int K) {
        const std::size_t N = static_cast<std::size_t>(state.range(0));
        auto handles = MakeHandles(N, K);

        BTBContract::Params p{0};

        // Warm-up: one full pass before timed loop
        for (std::size_t i = 0; i < N; ++i) { handles[i](p); benchmark::DoNotOptimize(p); }

        for (auto _ : state) {
            for (std::size_t i = 0; i < N; ++i) {
                handles[i](p);
                benchmark::DoNotOptimize(p);
            }
            benchmark::ClobberMemory();
        }

        const std::int64_t total =
            static_cast<std::int64_t>(state.iterations()) *
            static_cast<std::int64_t>(N);

        state.SetItemsProcessed(total);
        state.counters["targets_K"] = benchmark::Counter(static_cast<double>(K));
        state.counters["ns_per_call"] = benchmark::Counter(
            static_cast<double>(total),
            benchmark::Counter::kIsRate | benchmark::Counter::kInvert);
    }

} // namespace

#define BTB_RANGE() RangeMultiplier(8)->Range(1 << 10, 1 << 22)

static void BM_BTB_1Target  (benchmark::State& s) { RunBTBBench(s,  1); }
static void BM_BTB_4Targets (benchmark::State& s) { RunBTBBench(s,  4); }
static void BM_BTB_16Targets(benchmark::State& s) { RunBTBBench(s, 16); }
static void BM_BTB_64Targets(benchmark::State& s) { RunBTBBench(s, 64); }

BENCHMARK(BM_BTB_1Target)  ->BTB_RANGE();
BENCHMARK(BM_BTB_4Targets) ->BTB_RANGE();
BENCHMARK(BM_BTB_16Targets)->BTB_RANGE();
BENCHMARK(BM_BTB_64Targets)->BTB_RANGE();
