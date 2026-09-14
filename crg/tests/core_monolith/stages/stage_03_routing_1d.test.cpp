// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// STAGE 03 — 1-Dimensional Contextual Routing
// ─────────────────────────────────────────────────────────────────────────────
// PROBLEM:
//   The same entity type must behave differently depending on a runtime context
//   value — difficulty, game mode, weather, lod level. A switch/if tree would
//   branch; a hash map would scatter. Both defeat the branch predictor.
//
// SOLUTION:
//   Add an axis: an enum class + EnumTraits<> count specialization.
//   Specialize CapabilityRoutingTraits with CapabilitySpace<YourAxis>.
//   The tensor now has (models × axis_values) cells. The cell is selected by
//   Horner's method — a branchless multiply-add, computed at the call site.
//
//   The capability template is specialized on TAt to provide different behavior
//   per coordinate. The primary template is the default; partial specializations
//   on At<SomeValue> override specific cells.
//
// WHEN TO USE:
//   - One context dimension drives behavior (difficulty, quality preset, etc.)
//   - The set of values is known at compile time (bounded enum)
//
// WHAT'S NEW vs Stage 02:
//   EnumTraits, CapabilitySpace<Axis>, CapabilityRoutingTraits specialization,
//   TAt partial specialization on the capability template.
//   Find() gains one context argument.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

// ─── Domain + Model ───────────────────────────────────────────────────────────
struct S03Domain {};
CRG_DECLARE_DOMAIN(S03Domain)
CRG_DEFINE_DOMAIN(S03Domain)

struct S03Unit { u8 m_Data[8]; };
CRG_DECLARE_DOMAIN_MODELS(S03Domain,
    S03Unit)

// ─── Axis: the context dimension ──────────────────────────────────────────────
//   EnumTraits<T>::Count tells CapabilitySpace how many cells to allocate.
//   Must be a contiguous enum starting at 0.
enum class Difficulty03 { Easy, Hard };

namespace crg {
    template<> struct EnumTraits<Difficulty03> { static constexpr std::size_t Count = 2; };
}

// ─── Contract ─────────────────────────────────────────────────────────────────
struct IMove03 {
    struct Params { float m_Speed{ 0.f }; };
};

// ─── Routing traits: 1D tensor on Difficulty03 ────────────────────────────────
//   Without this specialization the default is CapabilitySpace<> (0D, Stage 02).
namespace crg::routing {
    template<> struct CapabilityRoutingTraits<S03Domain, IMove03> {
        using SpaceType = CapabilitySpace<Difficulty03>;
    };
}

// ─── Capability: primary template = default cell (Easy) ───────────────────────
template<typename TModel, typename TAt>
struct DifficultyMove : public Capability<IMove03> {
    static void Execute(IMove03::Params& p) { p.m_Speed = 1.0f; }
};

// ─── Partial specialization: override the Hard cell ───────────────────────────
//   At<Difficulty03::Hard> is the compile-time coordinate for the Hard cell.
//   The binding system instantiates this specialization for index 1 of the tensor.
template<typename TModel>
struct DifficultyMove<TModel, At<Difficulty03::Hard>> : public Capability<IMove03> {
    static void Execute(IMove03::Params& p) { p.m_Speed = 3.0f; }
};

namespace {
    static const CapabilityBinding<S03Domain, S03Unit, DifficultyMove> s_b;
}

// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Stage 03 — Tensor volume equals axis count", "[stage03][routing1d]") {
    using Space = CapabilityRoutingTraits<S03Domain, IMove03>::SpaceType;
    STATIC_REQUIRE(Space::Volume == 2);
    STATIC_REQUIRE(Space::Dimensions == 1);
}

TEST_CASE("Stage 03 — Different axis values route to different cells", "[stage03][routing1d]") {
    auto token = ModelToken<S03Domain>::FromType<S03Unit>();

    auto gEasy = CapabilityRouter<S03Domain>::Find<IMove03>(token, Difficulty03::Easy);
    auto gHard = CapabilityRouter<S03Domain>::Find<IMove03>(token, Difficulty03::Hard);

    REQUIRE(gEasy);
    REQUIRE(gHard);

    IMove03::Params easy, hard;
    gEasy(easy);
    gHard(hard);

    REQUIRE(easy.m_Speed == 1.0f); // primary template
    REQUIRE(hard.m_Speed == 3.0f); // partial specialization
}
