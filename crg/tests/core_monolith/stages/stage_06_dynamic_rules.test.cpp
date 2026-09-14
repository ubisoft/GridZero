// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// STAGE 06 — Specializable Dispatch Cells: EarlyExit & UtilityScoring
// ─────────────────────────────────────────────────────────────────────────────
// PROBLEM:
//   Dynamic rule evaluation is not one-size-fits-all.
//   - LiveOps overrides need a bool predicate with priority (first match wins).
//   - AI behavior selection needs float scoring (highest score wins).
//   Hard-coding one strategy into the tensor cell blocks the other.
//
// SOLUTION — Specializable DispatchCell:
//   CellSelector<TDomain, TContract> picks the cell type for each slot:
//     Default → DispatchCell       (Early-Exit: bool predicate, priority order)
//     Override → any custom cell injected via CapabilityRoutingTraits::DispatchCellType
//
//   The cell contract is two methods:
//     Bind<Impl>(target, instance)  — called once at arena-build time
//     Resolve(ctx)                  — called on every Find()
//
//   UtilityScoringCell (dispatch_strategies.hpp) is the built-in alternative.
//   Config requirement changes: Condition(ctx)->bool  →  Evaluate(ctx)->float
//
// WHAT'S NEW vs Stages 02–05:
//   DispatchCellType in routing traits. Two cell strategies in one test.
//   FillArena no longer writes m_DynamicRules directly — it calls cell.Bind.
//   Find no longer iterates m_DynamicRules directly — it calls cell.Resolve.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

// =============================================================================
// PART A — Early-Exit strategy (default DispatchCell)
// =============================================================================

struct S06AEEDomain {};
CRG_DECLARE_DOMAIN(S06AEEDomain)
CRG_DEFINE_DOMAIN(S06AEEDomain)

struct S06AEEUnit { u8 m_Data[8]; };
CRG_DECLARE_DOMAIN_MODELS(S06AEEDomain,
    S06AEEUnit)

// ─── Contract with RuleContext ────────────────────────────────────────────────
//   RuleContext is constructed from the extra args passed to Find().
//   For a 0D space, the routing offset ignores those args; they are used only
//   to build the context evaluated by each rule's predicate.
struct IFireControl {
    struct Params    { bool m_ShouldFire{ false }; };
    struct RuleContext {
        int m_ThreatLevel{ 0 };
        explicit RuleContext(int t) : m_ThreatLevel(t) {}
    };
};

// ─── Config: bool predicate + priority ───────────────────────────────────────
//   m_Priority  = evaluation order (higher = evaluated first)
//   Condition() = returns true when the rule applies
struct ThreatConfig {
    int m_MinThreat{ 0 };

    bool Condition(const IFireControl::RuleContext& ctx) const {
        return ctx.m_ThreatLevel >= m_MinThreat;
    }
};

// ─── Dynamic capability (fires above threshold) ───────────────────────────────
template<typename TModel>
struct AggressiveFire : public Capability<IFireControl, ThreatConfig> {
    AggressiveFire() {
        this->m_Config.m_MinThreat = 5;
    }
    static void Execute(IFireControl::Params& p) { p.m_ShouldFire = true; }
};

// ─── Static fallback capability (never fires) ─────────────────────────────────
template<typename TModel>
struct PassiveFire : public Capability<IFireControl> {
    static void Execute(IFireControl::Params& p) { p.m_ShouldFire = false; }
};

namespace {
    static const CapabilityBinding<S06AEEDomain, S06AEEUnit, AggressiveFire, PassiveFire> s_ee;
}

TEST_CASE("Stage 06A — EarlyExit: dynamic rule fires when condition is met", "[stage06][earlyexit]") {
    auto token = ModelToken<S06AEEDomain>::FromType<S06AEEUnit>();

    // ThreatLevel 7 >= MinThreat 5 → AggressiveFire matches → fires
    auto gate = CapabilityRouter<S06AEEDomain>::Find<IFireControl>(
                    token, IFireControl::RuleContext{7});
    REQUIRE(gate);

    IFireControl::Params p;
    gate(p);
    REQUIRE(p.m_ShouldFire == true);
}

TEST_CASE("Stage 06A — EarlyExit: static fallback used when no rule matches", "[stage06][earlyexit]") {
    auto token = ModelToken<S06AEEDomain>::FromType<S06AEEUnit>();

    // ThreatLevel 3 < MinThreat 5 → rule rejected → static fallback
    auto gate = CapabilityRouter<S06AEEDomain>::Find<IFireControl>(
                    token, IFireControl::RuleContext{3});
    REQUIRE(gate);

    IFireControl::Params p;
    gate(p);
    REQUIRE(p.m_ShouldFire == false);
}

// =============================================================================
// PART B — Utility Scoring strategy (injected UtilityScoringCell)
// =============================================================================

struct S06USDomain {};
CRG_DECLARE_DOMAIN(S06USDomain)
CRG_DEFINE_DOMAIN(S06USDomain)

struct S06USUnit { u8 m_Data[8]; };
CRG_DECLARE_DOMAIN_MODELS(S06USDomain,
    S06USUnit)

// ─── Contract ─────────────────────────────────────────────────────────────────
struct ICombatAI {
    struct Params    { const char* m_Strategy{ nullptr }; };
    struct RuleContext {
        float m_ThreatLevel{ 0.f };
        explicit RuleContext(float t) : m_ThreatLevel(t) {}
    };
};

// ─── Inject UtilityScoringCell for this domain+contract pair ──────────────────
//   CellSelector<S06USDomain, ICombatAI> will pick UtilityScoringCell instead of
//   the default DispatchCell. FillArena and Find are unchanged — the cell
//   interface (Bind / Resolve) is the same; only the strategy differs.
namespace crg::routing {
    template<> struct CapabilityRoutingTraits<S06USDomain, ICombatAI> {
        using SpaceType        = CapabilitySpace<>;
        using DispatchCellType = UtilityScoringCell<S06USDomain, ICombatAI>;
    };
}

// ─── Scorer configs: float Evaluate() ────────────────────────────────────────
struct AttackScorer {
    float Evaluate(const ICombatAI::RuleContext& ctx) const {
        return ctx.m_ThreatLevel * 0.8f; // scales with threat
    }
};
struct FleeScorer {
    float Evaluate(const ICombatAI::RuleContext& ctx) const {
        return 100.f - ctx.m_ThreatLevel; // scales against threat
    }
};

template<typename TModel>
struct AttackBehavior : public Capability<ICombatAI, AttackScorer> {
    static void Execute(ICombatAI::Params& p) { p.m_Strategy = "attack"; }
};

template<typename TModel>
struct FleeBehavior : public Capability<ICombatAI, FleeScorer> {
    static void Execute(ICombatAI::Params& p) { p.m_Strategy = "flee"; }
};

namespace {
    static const CapabilityBinding<S06USDomain, S06USUnit, AttackBehavior, FleeBehavior> s_us;
}

TEST_CASE("Stage 06B — UtilityScoring picks highest-score capability", "[stage06][utility]") {
    auto token = ModelToken<S06USDomain>::FromType<S06USUnit>();

    // High threat (80): Attack = 0.8×80 = 64, Flee = 100-80 = 20 → Attack wins
    {
        auto gate = CapabilityRouter<S06USDomain>::Find<ICombatAI>(
                        token, ICombatAI::RuleContext{80.f});
        REQUIRE(gate);
        ICombatAI::Params p;
        gate(p);
        REQUIRE(std::string_view{p.m_Strategy} == "attack");
    }

    // Low threat (10): Attack = 0.8×10 = 8, Flee = 100-10 = 90 → Flee wins
    {
        auto gate = CapabilityRouter<S06USDomain>::Find<ICombatAI>(
                        token, ICombatAI::RuleContext{10.f});
        REQUIRE(gate);
        ICombatAI::Params p;
        gate(p);
        REQUIRE(std::string_view{p.m_Strategy} == "flee");
    }
}

TEST_CASE("Stage 06B — UtilityScoringCell selected at compile time", "[stage06][utility]") {
    using Selected = CellSelector<S06USDomain, ICombatAI>::Type;
    STATIC_REQUIRE(std::is_same_v<Selected, UtilityScoringCell<S06USDomain, ICombatAI>>);

    // Default (no DispatchCellType) still resolves to DispatchCell
    using Default = CellSelector<S06AEEDomain, IFireControl>::Type;
    STATIC_REQUIRE(std::is_same_v<Default, DispatchCell<S06AEEDomain, IFireControl>>);
}

// =============================================================================
// PART C — Combined N-D axes + RuleContext
// =============================================================================

struct S06CDomain {};
CRG_DECLARE_DOMAIN(S06CDomain)
CRG_DEFINE_DOMAIN(S06CDomain)

struct S06CUnit { u8 m_Data[8]; };
CRG_DECLARE_DOMAIN_MODELS(S06CDomain,
    S06CUnit)

enum class Terrain06 { Flat, Rough };

namespace crg {
    template<> struct EnumTraits<Terrain06> { static constexpr std::size_t Count = 2; };
}

struct IPatrol06 {
    struct Params    { bool m_ShouldEngage{ false }; };
    struct RuleContext {
        int m_ThreatLevel{ 0 };
        explicit RuleContext(int t) : m_ThreatLevel(t) {}
    };
};

namespace crg::routing {
    template<> struct CapabilityRoutingTraits<S06CDomain, IPatrol06> {
        using SpaceType = CapabilitySpace<Terrain06>;
    };
}

struct PatrolEngageConfig {
    int m_MinThreat{ 0 };

    bool Condition(const IPatrol06::RuleContext& ctx) const {
        return ctx.m_ThreatLevel >= m_MinThreat;
    }
};

template<typename TModel, typename TAt>
struct AggressivePatrol : public Capability<IPatrol06, PatrolEngageConfig> {
    AggressivePatrol() {
        this->m_Config.m_MinThreat = 5;
    }
    static void Execute(IPatrol06::Params& p) { p.m_ShouldEngage = true; }
};

template<typename TModel, typename TAt>
struct PassivePatrol : public Capability<IPatrol06> {
    static void Execute(IPatrol06::Params& p) { p.m_ShouldEngage = false; }
};

namespace {
    static const CapabilityBinding<S06CDomain, S06CUnit, AggressivePatrol, PassivePatrol> s_patrol;
}

TEST_CASE("Stage 06C — combined axes+RuleContext: axis selects cell", "[stage06][combined]") {
    using TSpace = CapabilityRoutingTraits<S06CDomain, IPatrol06>::SpaceType;
    STATIC_REQUIRE(TSpace::Dimensions == 1);

    REQUIRE(TSpace::ComputeOffset(0, Terrain06::Flat) != TSpace::ComputeOffset(0, Terrain06::Rough));
}

TEST_CASE("Stage 06C — combined axes+RuleContext: rule gates within each axis cell", "[stage06][combined]") {
    auto token = ModelToken<S06CDomain>::FromType<S06CUnit>();

    for (auto terrain : { Terrain06::Flat, Terrain06::Rough }) {
        {
            auto gate = CapabilityRouter<S06CDomain>::Find<IPatrol06>(
                            token, IPatrol06::RuleContext{7}, terrain);
            REQUIRE(gate);
            IPatrol06::Params p;
            gate(p);
            REQUIRE(p.m_ShouldEngage == true);
        }
        {
            auto gate = CapabilityRouter<S06CDomain>::Find<IPatrol06>(
                            token, IPatrol06::RuleContext{2}, terrain);
            REQUIRE(gate);
            IPatrol06::Params p;
            gate(p);
            REQUIRE(p.m_ShouldEngage == false);
        }
    }
}

// =============================================================================
// PART D — Find() does not copy RuleContext
// =============================================================================

struct S06DDomain {};
CRG_DECLARE_DOMAIN(S06DDomain)
CRG_DEFINE_DOMAIN(S06DDomain)

struct S06DUnit { u8 m_Data[8]; };
CRG_DECLARE_DOMAIN_MODELS(S06DDomain,
    S06DUnit)

struct IAlert06 {
    struct Params    { bool m_Raised{ false }; };
    struct RuleContext {
        static inline int s_CopyCount{ 0 };

        int m_Level{ 0 };
        explicit RuleContext(int l) : m_Level(l) {}
        RuleContext(const RuleContext& other) : m_Level(other.m_Level) { ++s_CopyCount; }
        RuleContext(RuleContext&&) = default;
    };
};

struct AlertConfig {
    bool Condition(const IAlert06::RuleContext& ctx) const {
        return ctx.m_Level > 0;
    }
};

template<typename TModel>
struct RaiseAlert : public Capability<IAlert06, AlertConfig> {
    static void Execute(IAlert06::Params& p) { p.m_Raised = true; }
};

template<typename TModel>
struct ClearAlert : public Capability<IAlert06> {
    static void Execute(IAlert06::Params& p) { p.m_Raised = false; }
};

namespace {
    static const CapabilityBinding<S06DDomain, S06DUnit, RaiseAlert, ClearAlert> s_alert;
}

TEST_CASE("Stage 06D — Find() does not copy RuleContext", "[stage06][nocopy]") {
    auto token = ModelToken<S06DDomain>::FromType<S06DUnit>();

    IAlert06::RuleContext::s_CopyCount = 0;
    IAlert06::RuleContext ctx{ 1 };

    auto gate = CapabilityRouter<S06DDomain>::Find<IAlert06>(token, ctx);
    REQUIRE(gate);

    REQUIRE(IAlert06::RuleContext::s_CopyCount == 0);
}
