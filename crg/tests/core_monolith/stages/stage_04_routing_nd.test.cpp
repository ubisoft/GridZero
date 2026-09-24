// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// STAGE 04 — N-Dimensional Contextual Routing (Horner's Method)
// ─────────────────────────────────────────────────────────────────────────────
// PROBLEM:
//   Behavior depends on multiple independent context axes simultaneously
//   (Battery × Terrain × Mode). A nested if/switch tree grows as O(N1×N2×N3).
//   A hash map collapses dimensions but adds a per-call hashing cost.
//
// SOLUTION:
//   CapabilitySpace<A, B, C…> extends to N dimensions. The tensor index is
//   computed via Horner's method: offset = ((v0 × |A1|) + v1) × |A2| + v2 …
//   This is a branchless multiply-add loop: one pass, no branching, no hashing.
//   Volume = |A0| × |A1| × … × |AN-1| cells. Everything is compile-time.
//
//   The API is *identical* to Stage 03 — Find() gains one extra arg per axis.
//   TAt carries the full N-dimensional coordinate for specialization.
//
// WHEN TO USE:
//   - Multiple independent context dimensions affect behavior simultaneously
//   - The total volume stays tractable (avoid combinatorial explosion)
//
// WHAT'S NEW vs Stage 03:
//   Multiple axes in CapabilitySpace. Find() takes N context args.
//   TAt becomes At<v0, v1, v2> — a multi-dimensional coordinate type.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

// ─── Domain + Model ───────────────────────────────────────────────────────────
struct S04Domain {};
CRG_DECLARE_DOMAIN(S04Domain)
CRG_DEFINE_DOMAIN(S04Domain)

struct S04Unit { u8 m_Data[8]; };
CRG_DECLARE_DOMAIN_MODELS(S04Domain,
    S04Unit)

// ─── Three independent axes ───────────────────────────────────────────────────
enum class Battery04 { Critical, Low, Nominal };
enum class Terrain04 { Flat, Rough };
enum class Mode04    { Eco, Performance };

namespace crg {
    template<> struct EnumTraits<Battery04> { static constexpr std::size_t Count = 3; };
    template<> struct EnumTraits<Terrain04> { static constexpr std::size_t Count = 2; };
    template<> struct EnumTraits<Mode04>    { static constexpr std::size_t Count = 2; };
}

// ─── Contract: Brain (virtual), same shape as Stages 02-03 ────────────────────
struct ILocomotion04 {
    virtual void Drive(float& outSpeed, bool& outActive) const = 0;
    virtual ~ILocomotion04() = default;
};

// ─── Routing traits: 3D tensor — 3 × 2 × 2 = 12 cells ───────────────────────
namespace crg::routing {
    template<> struct CapabilityRoutingTraits<S04Domain, ILocomotion04> {
        using SpaceType = CapabilitySpace<Battery04, Terrain04, Mode04>;
    };
}

// ─── Primary template: default behavior for all 12 cells ─────────────────────
template<typename TModel, typename TAt>
struct DriveCapability : public Capability<ILocomotion04> {
    void Drive(float& outSpeed, bool& outActive) const override {
        outSpeed  = 1.0f;
        outActive = true;
    }
};

// ─── Override the peak-performance corner: Nominal × Flat × Performance ───────
//   At<Battery04::Nominal, Terrain04::Flat, Mode04::Performance> is the
//   compile-time coordinate for tensor index 9: Horner applied left-to-right
//   over indices (2, 0, 1) against counts (3, 2, 2) — ((2 * 2) + 0) * 2 + 1 = 9.
template<typename TModel>
struct DriveCapability<TModel, At<Battery04::Nominal, Terrain04::Flat, Mode04::Performance>>
    : public Capability<ILocomotion04> {
    void Drive(float& outSpeed, bool& outActive) const override {
        outSpeed  = 10.0f;
        outActive = true;
    }
};

namespace {
    static const CapabilityBinding<S04Domain, S04Unit, DriveCapability> s_b;
}

// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Stage 04 — 3D tensor volume is the product of axis counts", "[stage04][routingNd]") {
    using Space = CapabilityRoutingTraits<S04Domain, ILocomotion04>::SpaceType;
    STATIC_REQUIRE(Space::Volume == 12); // 3 × 2 × 2
    STATIC_REQUIRE(Space::Dimensions == 3);
}

TEST_CASE("Stage 04 — Default cell dispatches via primary template", "[stage04][routingNd]") {
    auto token = ModelToken<S04Domain>::FromType<S04Unit>();
    auto gate  = CapabilityRouter<S04Domain>::Find<ILocomotion04>(
                      token, Battery04::Critical, Terrain04::Rough, Mode04::Eco);
    REQUIRE(gate);

    float speed = 0.f;
    bool  active = false;
    gate->Drive(speed, active);

    REQUIRE(speed  == 1.0f);
    REQUIRE(active == true);
}

TEST_CASE("Stage 04 — Specialised corner cell dispatches via partial specialization", "[stage04][routingNd]") {
    auto token = ModelToken<S04Domain>::FromType<S04Unit>();
    auto gate  = CapabilityRouter<S04Domain>::Find<ILocomotion04>(
                      token, Battery04::Nominal, Terrain04::Flat, Mode04::Performance);
    REQUIRE(gate);

    float speed = 0.f;
    bool  active = false;
    gate->Drive(speed, active);

    REQUIRE(speed  == 10.0f);
    REQUIRE(active == true);
}

TEST_CASE("Stage 04 — All 12 cells are populated after first Find", "[stage04][routingNd]") {
    auto token = ModelToken<S04Domain>::FromType<S04Unit>();
    CapabilityRouter<S04Domain>::Find<ILocomotion04>(
        token, Battery04::Nominal, Terrain04::Flat, Mode04::Eco);

    REQUIRE(TensorArena<S04Domain, ILocomotion04>::GetSize() == 12);
}
