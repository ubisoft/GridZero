// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// DISPATCH CELL STRATEGIES — UtilityScoringCell + combined N-D axes/RuleContext
// ─────────────────────────────────────────────────────────────────────────────
// CellSelector<TDomain, TContract> picks the cell type for each slot: the
// default DispatchCell (Early-Exit: bool predicate, priority order), or any
// cell injected via CapabilityRoutingTraits::DispatchCellType. UtilityScoring
// is the built-in float-scoring alternative (highest score wins). This file
// also covers a routing axis combined with a RuleContext on the same
// contract — the axis selects the cell, the RuleContext's predicate gates
// within that cell.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

// =============================================================================
// PART — UtilityScoring strategy (injected UtilityScoringCell)
// =============================================================================

namespace crg::test::dispatch_cell_strategies {

    struct UtilityScoringDomain {};
    struct UtilityScoringUnit { u8 m_Data[8]; };

    struct ICombatAI {
        struct Params    { const char* m_Strategy{ nullptr }; };
        struct RuleContext {
            float m_ThreatLevel{ 0.f };
            explicit RuleContext(float t) : m_ThreatLevel(t) {}
        };
    };

    // ─── A default-cell contract, for contrast with the injected cell below ───
    struct IDefaultCellProbe {
        struct Params { bool m_Dummy{ false }; };
    };

} // namespace crg::test::dispatch_cell_strategies

CRG_DECLARE_DOMAIN(crg::test::dispatch_cell_strategies::UtilityScoringDomain)
CRG_DEFINE_DOMAIN(crg::test::dispatch_cell_strategies::UtilityScoringDomain)

CRG_DECLARE_DOMAIN_MODELS(crg::test::dispatch_cell_strategies::UtilityScoringDomain,
    crg::test::dispatch_cell_strategies::UtilityScoringUnit)

// ─── Inject UtilityScoringCell for this domain+contract pair ──────────────────
//   CellSelector<UtilityScoringDomain, ICombatAI> will pick UtilityScoringCell
//   instead of the default DispatchCell. FillArena and Find are unchanged —
//   the cell interface (Bind / Resolve) is the same; only the strategy differs.
namespace crg::routing {
    template<> struct CapabilityRoutingTraits<
        crg::test::dispatch_cell_strategies::UtilityScoringDomain,
        crg::test::dispatch_cell_strategies::ICombatAI> {
        using SpaceType        = CapabilitySpace<>;
        using DispatchCellType = UtilityScoringCell<
            crg::test::dispatch_cell_strategies::UtilityScoringDomain,
            crg::test::dispatch_cell_strategies::ICombatAI>;
    };
}

namespace crg::test::dispatch_cell_strategies {

    // ─── Scorer configs: float Evaluate() ────────────────────────────────────
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

} // namespace crg::test::dispatch_cell_strategies

namespace {
    using namespace crg::test::dispatch_cell_strategies;
    static const CapabilityBinding<UtilityScoringDomain, UtilityScoringUnit, AttackBehavior, FleeBehavior> s_us;
}

TEST_CASE("Dispatch cell strategies — UtilityScoring picks highest-score capability", "[dispatch_cell_strategies][utility]") {
    using namespace crg::test::dispatch_cell_strategies;
    auto token = ModelToken<UtilityScoringDomain>::FromType<UtilityScoringUnit>();

    // High threat (80): Attack = 0.8×80 = 64, Flee = 100-80 = 20 → Attack wins
    {
        auto gate = CapabilityRouter<UtilityScoringDomain>::Find<ICombatAI>(
                        token, ICombatAI::RuleContext{80.f});
        REQUIRE(gate);
        ICombatAI::Params p;
        gate(p);
        REQUIRE(std::string_view{p.m_Strategy} == "attack");
    }

    // Low threat (10): Attack = 0.8×10 = 8, Flee = 100-10 = 90 → Flee wins
    {
        auto gate = CapabilityRouter<UtilityScoringDomain>::Find<ICombatAI>(
                        token, ICombatAI::RuleContext{10.f});
        REQUIRE(gate);
        ICombatAI::Params p;
        gate(p);
        REQUIRE(std::string_view{p.m_Strategy} == "flee");
    }
}

TEST_CASE("Dispatch cell strategies — UtilityScoringCell selected at compile time", "[dispatch_cell_strategies][utility]") {
    using namespace crg::test::dispatch_cell_strategies;
    using Selected = CellSelector<UtilityScoringDomain, ICombatAI>::Type;
    STATIC_REQUIRE(std::is_same_v<Selected, UtilityScoringCell<UtilityScoringDomain, ICombatAI>>);

    // Default (no DispatchCellType) still resolves to DispatchCell.
    using Default = CellSelector<UtilityScoringDomain, IDefaultCellProbe>::Type;
    STATIC_REQUIRE(std::is_same_v<Default, DispatchCell<UtilityScoringDomain, IDefaultCellProbe>>);
}

// =============================================================================
// PART — Combined N-D axes + RuleContext
// =============================================================================

namespace crg::test::dispatch_cell_strategies {

    struct PatrolDomain {};
    struct PatrolUnit { u8 m_Data[8]; };

    enum class PatrolTerrain { Flat, Rough };

} // namespace crg::test::dispatch_cell_strategies

namespace crg {
    template<> struct EnumTraits<crg::test::dispatch_cell_strategies::PatrolTerrain> {
        static constexpr std::size_t Count = 2;
    };
}

CRG_DECLARE_DOMAIN(crg::test::dispatch_cell_strategies::PatrolDomain)
CRG_DEFINE_DOMAIN(crg::test::dispatch_cell_strategies::PatrolDomain)

CRG_DECLARE_DOMAIN_MODELS(crg::test::dispatch_cell_strategies::PatrolDomain,
    crg::test::dispatch_cell_strategies::PatrolUnit)

namespace crg::test::dispatch_cell_strategies {

    struct IPatrol {
        struct Params    { bool m_ShouldEngage{ false }; };
        struct RuleContext {
            int m_ThreatLevel{ 0 };
            explicit RuleContext(int t) : m_ThreatLevel(t) {}
        };
    };

} // namespace crg::test::dispatch_cell_strategies

namespace crg::routing {
    template<> struct CapabilityRoutingTraits<
        crg::test::dispatch_cell_strategies::PatrolDomain,
        crg::test::dispatch_cell_strategies::IPatrol> {
        using SpaceType = CapabilitySpace<crg::test::dispatch_cell_strategies::PatrolTerrain>;
    };
}

namespace crg::test::dispatch_cell_strategies {

    struct PatrolEngageConfig {
        int m_MinThreat{ 0 };

        bool Condition(const IPatrol::RuleContext& ctx) const {
            return ctx.m_ThreatLevel >= m_MinThreat;
        }
    };

    template<typename TModel, typename TAt>
    struct AggressivePatrol : public Capability<IPatrol, PatrolEngageConfig> {
        AggressivePatrol() {
            this->m_Config.m_MinThreat = 5;
        }
        static void Execute(IPatrol::Params& p) { p.m_ShouldEngage = true; }
    };

    template<typename TModel, typename TAt>
    struct PassivePatrol : public Capability<IPatrol> {
        static void Execute(IPatrol::Params& p) { p.m_ShouldEngage = false; }
    };

} // namespace crg::test::dispatch_cell_strategies

namespace {
    using namespace crg::test::dispatch_cell_strategies;
    static const CapabilityBinding<PatrolDomain, PatrolUnit, AggressivePatrol, PassivePatrol> s_patrol;
}

TEST_CASE("Dispatch cell strategies — combined axes+RuleContext: axis selects cell", "[dispatch_cell_strategies][combined]") {
    using namespace crg::test::dispatch_cell_strategies;
    using TSpace = CapabilityRoutingTraits<PatrolDomain, IPatrol>::SpaceType;
    STATIC_REQUIRE(TSpace::Dimensions == 1);

    REQUIRE(TSpace::ComputeOffset(0, PatrolTerrain::Flat) != TSpace::ComputeOffset(0, PatrolTerrain::Rough));
}

TEST_CASE("Dispatch cell strategies — combined axes+RuleContext: rule gates within each axis cell", "[dispatch_cell_strategies][combined]") {
    using namespace crg::test::dispatch_cell_strategies;
    auto token = ModelToken<PatrolDomain>::FromType<PatrolUnit>();

    for (auto terrain : { PatrolTerrain::Flat, PatrolTerrain::Rough }) {
        {
            auto gate = CapabilityRouter<PatrolDomain>::Find<IPatrol>(
                            token, IPatrol::RuleContext{7}, terrain);
            REQUIRE(gate);
            IPatrol::Params p;
            gate(p);
            REQUIRE(p.m_ShouldEngage == true);
        }
        {
            auto gate = CapabilityRouter<PatrolDomain>::Find<IPatrol>(
                            token, IPatrol::RuleContext{2}, terrain);
            REQUIRE(gate);
            IPatrol::Params p;
            gate(p);
            REQUIRE(p.m_ShouldEngage == false);
        }
    }
}
