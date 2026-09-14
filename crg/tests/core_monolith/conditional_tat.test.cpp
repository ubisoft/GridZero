// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// CONDITIONAL TAt — Pure Dimensionality Model compile gates
// ─────────────────────────────────────────────────────────────────────────────
// CapabilityNode/CapabilityBinding (capability_binding.hpp) route TAt strictly
// by CapabilityRoutingTraits<...>::SpaceType::Dimensions:
//
//     Dimensions == 0   ->   TCap<TModel>
//     Dimensions  > 0   ->   TCap<TModel, MakeAt<Space, TIs>>
//
// Arity is never a dispatch input. These gates verify the seven hard cases the
// design was checked against before implementation, plus the one thing pure
// reasoning cannot settle: whether MSVC treats a 1-arg probe against a >=2-arg
// primary as a substitution failure (SFINAE) rather than a hard error. That
// property is exercised by every Dims>0 gate below (2, 4, 5, 6) via
// CapContract's std::void_t probe in capability_binding.hpp -- if MSVC got it
// wrong, this file would fail to compile, not fail at runtime.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

#include <type_traits>

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

// ─── Domain + models shared by every gate below ───────────────────────────────
struct TatGateDomain {};
CRG_DECLARE_DOMAIN(TatGateDomain)
CRG_DEFINE_DOMAIN(TatGateDomain)

struct TatGateSfinaeBase {};
struct TatGateUnit { u8 m_Data[8]; };
struct TatGateSfinaeMatchModel : public TatGateSfinaeBase { u8 m_Data[8]; };

CRG_DECLARE_DOMAIN_MODELS(TatGateDomain,
    TatGateUnit,
    TatGateSfinaeMatchModel)

// Deliberately never registered/bound — see Gate 3. Proving it is REJECTED
// must not require an actual failing CapabilityBinding instantiation (that
// would hard-error the whole translation unit), so it is checked via a
// standalone void_t probe below instead.
struct TatGateSfinaeMismatchModel { u8 m_Data[8]; };

// ─── Gate 1 — 0-D 1-arg capability ─────────────────────────────────────────────
struct ITatGateNoAxis {
    struct Params { float m_Value{ 0.f }; };
};

template<typename TModel>
struct TatGateNoAxisCap : public Capability<ITatGateNoAxis> {
    static void Execute(ITatGateNoAxis::Params& p) { p.m_Value = 1.0f; }
};

namespace { static const CapabilityBinding<TatGateDomain, TatGateUnit, TatGateNoAxisCap> s_TatGateNoAxisBinding; }

static_assert(std::is_same_v<CapImplAt<TatGateDomain, TatGateUnit, TatGateNoAxisCap, 0>, TatGateNoAxisCap<TatGateUnit>>,
    "Gate 1: Dimensions == 0 must select the 1-arg TCap<TModel> form.");

TEST_CASE("ConditionalTAt Gate 1 -- 0-D 1-arg capability compiles, binds, routes, executes", "[conditional_tat][gate1]") {
    auto token = ModelToken<TatGateDomain>::FromType<TatGateUnit>();
    auto gate  = CapabilityRouter<TatGateDomain>::Find<ITatGateNoAxis>(token);
    REQUIRE(gate);

    ITatGateNoAxis::Params p;
    gate(p);
    REQUIRE(p.m_Value == 1.0f);
}

// ─── Gate 2 + Gate 5 — Dims>0 capability, At partial specialization ───────────
enum class TatGateAxis { A, B, C };
namespace crg {
    template<> struct EnumTraits<TatGateAxis> { static constexpr std::size_t Count = 3; };
}

struct ITatGateAxis {
    struct Params { int m_Value{ 0 }; };
};

namespace crg::routing {
    template<> struct CapabilityRoutingTraits<TatGateDomain, ITatGateAxis> {
        using SpaceType = CapabilitySpace<TatGateAxis>;
    };
}

template<typename TModel, typename TAt>
struct TatGateAxisCap : public Capability<ITatGateAxis> {
    static void Execute(ITatGateAxis::Params& p) { p.m_Value = 1; }
};

// Gate 5: the matching coordinate must route through this specialization.
template<typename TModel>
struct TatGateAxisCap<TModel, At<TatGateAxis::B>> : public Capability<ITatGateAxis> {
    static void Execute(ITatGateAxis::Params& p) { p.m_Value = 2; }
};

namespace { static const CapabilityBinding<TatGateDomain, TatGateUnit, TatGateAxisCap> s_TatGateAxisBinding; }

// Gate 2: CapImplAt resolves to the 2-arg TCap<TModel, MakeAt<Space,TIs>> form.
static_assert(std::is_same_v<
    CapImplAt<TatGateDomain, TatGateUnit, TatGateAxisCap, 1>,
    TatGateAxisCap<TatGateUnit, MakeAt<CapabilityRoutingTraits<TatGateDomain, ITatGateAxis>::SpaceType, 1>>>,
    "Gate 2: Dimensions > 0 must select the 2-arg TCap<TModel, MakeAt<Space,TIs>> form.");
static_assert(std::is_same_v<
    CapImplAt<TatGateDomain, TatGateUnit, TatGateAxisCap, 1>,
    TatGateAxisCap<TatGateUnit, At<TatGateAxis::B>>>,
    "Gate 5: index 1 (TatGateAxis::B) must resolve to the matching At<> partial specialization.");

TEST_CASE("ConditionalTAt Gate 2/5 -- Dims>0 capability binds per-coordinate via At partial specialization", "[conditional_tat][gate2][gate5]") {
    auto token = ModelToken<TatGateDomain>::FromType<TatGateUnit>();

    auto gateA = CapabilityRouter<TatGateDomain>::Find<ITatGateAxis>(token, TatGateAxis::A);
    ITatGateAxis::Params pa; gateA(pa);
    REQUIRE(pa.m_Value == 1);

    auto gateB = CapabilityRouter<TatGateDomain>::Find<ITatGateAxis>(token, TatGateAxis::B);
    ITatGateAxis::Params pb; gateB(pb);
    REQUIRE(pb.m_Value == 2);

    auto gateC = CapabilityRouter<TatGateDomain>::Find<ITatGateAxis>(token, TatGateAxis::C);
    ITatGateAxis::Params pc; gateC(pc);
    REQUIRE(pc.m_Value == 1);
}

// ─── Gate 3 — TModel enable_if SFINAE (0-D) ────────────────────────────────────
struct ITatGateSfinae0D {
    struct Params { bool m_Matched{ false }; };
};

template<typename TModel, typename = std::enable_if_t<std::is_base_of_v<TatGateSfinaeBase, TModel>>>
struct TatGateSfinaeCap : public Capability<ITatGateSfinae0D> {
    static void Execute(ITatGateSfinae0D::Params& p) { p.m_Matched = true; }
};

namespace { static const CapabilityBinding<TatGateDomain, TatGateSfinaeMatchModel, TatGateSfinaeCap> s_TatGateSfinaeBinding; }

template<typename TModel, typename = void>
struct TatGateSfinaeWellFormed : std::false_type {};
template<typename TModel>
struct TatGateSfinaeWellFormed<TModel, std::void_t<TatGateSfinaeCap<TModel>>> : std::true_type {};

static_assert(TatGateSfinaeWellFormed<TatGateSfinaeMatchModel>::value,
    "Gate 3: a model satisfying the author's own enable_if must form TCap<TModel>.");
static_assert(!TatGateSfinaeWellFormed<TatGateSfinaeMismatchModel>::value,
    "Gate 3: a model failing the author's own enable_if must NOT form TCap<TModel> -- "
    "pure-A never falls back to a 2nd arity, so it can never silently defeat user SFINAE.");

TEST_CASE("ConditionalTAt Gate 3 -- TModel enable_if SFINAE binds a matching model (0-D)", "[conditional_tat][gate3]") {
    auto token = ModelToken<TatGateDomain>::FromType<TatGateSfinaeMatchModel>();
    auto gate  = CapabilityRouter<TatGateDomain>::Find<ITatGateSfinae0D>(token);
    REQUIRE(gate);

    ITatGateSfinae0D::Params p;
    gate(p);
    REQUIRE(p.m_Matched);
}

// ─── Gate 4 — TModel enable_if + TAt (Dims>0) ──────────────────────────────────
struct ITatGateSfinaeAxis {
    struct Params { bool m_Matched{ false }; };
};

namespace crg::routing {
    template<> struct CapabilityRoutingTraits<TatGateDomain, ITatGateSfinaeAxis> {
        using SpaceType = CapabilitySpace<TatGateAxis>;
    };
}

template<typename TModel, typename TAt, typename = std::enable_if_t<std::is_base_of_v<TatGateSfinaeBase, TModel>>>
struct TatGateSfinaeAxisCap : public Capability<ITatGateSfinaeAxis> {
    static void Execute(ITatGateSfinaeAxis::Params& p) { p.m_Matched = true; }
};

namespace { static const CapabilityBinding<TatGateDomain, TatGateSfinaeMatchModel, TatGateSfinaeAxisCap> s_TatGateSfinaeAxisBinding; }

// Gate 4 + Gate 7: the bootstrap reader must resolve the contract through the
// enable_if-decorated 2-arg primary -- i.e. MSVC's 1-arg probe on this >=2-arg
// primary SFINAE'd away rather than hard-erroring (Gate 7's whole point).
static_assert(std::is_same_v<CapContractType<TatGateSfinaeMatchModel, TatGateSfinaeAxisCap>, ITatGateSfinaeAxis>,
    "Gate 4/7: the bootstrap contract-reader must resolve a 2-arg SFINAE-decorated primary.");

TEST_CASE("ConditionalTAt Gate 4 -- TModel enable_if + TAt binds a matching model through the 2-arg path", "[conditional_tat][gate4]") {
    auto token = ModelToken<TatGateDomain>::FromType<TatGateSfinaeMatchModel>();

    for (auto axis : { TatGateAxis::A, TatGateAxis::B, TatGateAxis::C }) {
        auto gate = CapabilityRouter<TatGateDomain>::Find<ITatGateSfinaeAxis>(token, axis);
        REQUIRE(gate);
        ITatGateSfinaeAxis::Params p;
        gate(p);
        REQUIRE(p.m_Matched);
    }
}

// ─── Gate 6 — At<auto...> variadic-prefix specialization ──────────────────────
enum class TatGateVarAxisA { P, Q };
enum class TatGateVarAxisB { R, S };
namespace crg {
    template<> struct EnumTraits<TatGateVarAxisA> { static constexpr std::size_t Count = 2; };
    template<> struct EnumTraits<TatGateVarAxisB> { static constexpr std::size_t Count = 2; };
}

struct ITatGateVariadic {
    struct Params { int m_Value{ 0 }; };
};

namespace crg::routing {
    template<> struct CapabilityRoutingTraits<TatGateDomain, ITatGateVariadic> {
        using SpaceType = CapabilitySpace<TatGateVarAxisA, TatGateVarAxisB>;
    };
}

template<typename TModel, typename TAt>
struct TatGateVariadicCap : public Capability<ITatGateVariadic> {
    static void Execute(ITatGateVariadic::Params& p) { p.m_Value = 0; }
};

// Matches ANY coordinate whose first axis is P, regardless of the second axis
// -- the exact "template<class TModel, auto... TOther> Cap<TModel, At<Where::X,
// TOther...>>" shape flagged as a hard case before implementation began.
template<typename TModel, auto... TOther>
struct TatGateVariadicCap<TModel, At<TatGateVarAxisA::P, TOther...>> : public Capability<ITatGateVariadic> {
    static void Execute(ITatGateVariadic::Params& p) { p.m_Value = 42; }
};

namespace { static const CapabilityBinding<TatGateDomain, TatGateUnit, TatGateVariadicCap> s_TatGateVariadicBinding; }

TEST_CASE("ConditionalTAt Gate 6 -- At<auto...> variadic-prefix specialization matches any suffix", "[conditional_tat][gate6]") {
    auto token = ModelToken<TatGateDomain>::FromType<TatGateUnit>();

    auto gatePR = CapabilityRouter<TatGateDomain>::Find<ITatGateVariadic>(token, TatGateVarAxisA::P, TatGateVarAxisB::R);
    ITatGateVariadic::Params pr; gatePR(pr);
    REQUIRE(pr.m_Value == 42);

    auto gatePS = CapabilityRouter<TatGateDomain>::Find<ITatGateVariadic>(token, TatGateVarAxisA::P, TatGateVarAxisB::S);
    ITatGateVariadic::Params ps; gatePS(ps);
    REQUIRE(ps.m_Value == 42);

    auto gateQR = CapabilityRouter<TatGateDomain>::Find<ITatGateVariadic>(token, TatGateVarAxisA::Q, TatGateVarAxisB::R);
    ITatGateVariadic::Params qr; gateQR(qr);
    REQUIRE(qr.m_Value == 0);
}
