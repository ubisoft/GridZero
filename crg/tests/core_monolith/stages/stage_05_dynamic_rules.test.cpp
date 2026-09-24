// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// STAGE 05 — Dynamic Rules: EarlyExit dispatch cell (default), RuleContext
// ─────────────────────────────────────────────────────────────────────────────
// PROBLEM:
//   Several candidate capabilities can serve the same cell, but only one
//   should run — the choice depends on runtime data (a RuleContext), not on
//   the compile-time model/axis coordinate alone.
//
// SOLUTION — the default DispatchCell (EarlyExit):
//   Each candidate capability carries a Config with a Condition(RuleContext)
//   predicate. Find() walks candidates in binding order and returns the
//   first whose Condition() is true — "early exit", not a full scan+score.
//   A capability with no Config (no Condition) always matches: it is the
//   fallback/default entry.
//
//   Other dispatch-cell strategies (UtilityScoring, RuleContext across a
//   combined N-D axis, the no-copy guarantee) live in
//   routing/dispatch_cell_strategies.test.cpp and routing/rule_context.test.cpp
//   — this file keeps only the default EarlyExit cell, the shape every stage
//   before this one already relies on implicitly.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

// ─── Domain + Model ───────────────────────────────────────────────────────────
struct S05DomainRules {};
CRG_DECLARE_DOMAIN(S05DomainRules)
CRG_DEFINE_DOMAIN(S05DomainRules)

struct S05UnitRules { u8 m_Data[8]; };
CRG_DECLARE_DOMAIN_MODELS(S05DomainRules,
    S05UnitRules)

// ─── Contract: Params + RuleContext ───────────────────────────────────────────
struct IFireControl {
    struct Params { bool m_ShouldFire{ false }; };
    struct RuleContext {
        int m_ThreatLevel{ 0 };
        explicit RuleContext(int t) : m_ThreatLevel(t) {}
    };
};

// ─── Config: Condition(RuleContext) predicate ─────────────────────────────────
struct ThreatConfig {
    int m_MinThreat{ 0 };

    bool Condition(const IFireControl::RuleContext& ctx) const {
        return ctx.m_ThreatLevel >= m_MinThreat;
    }
};

// ─── Two candidates: aggressive (guarded), passive (fallback, no Config) ─────
template<typename TModel>
struct AggressiveFire : public Capability<IFireControl, ThreatConfig> {
    AggressiveFire() {
        this->m_Config.m_MinThreat = 5;
    }
    static void Execute(IFireControl::Params& p) { p.m_ShouldFire = true; }
};

template<typename TModel>
struct PassiveFire : public Capability<IFireControl> {
    static void Execute(IFireControl::Params& p) { p.m_ShouldFire = false; }
};

// ─── Binding order matters: guarded candidate first, fallback last ───────────
namespace {
    static const CapabilityBinding<S05DomainRules, S05UnitRules, AggressiveFire, PassiveFire> s_FireBinding;
}

// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Stage 05 — EarlyExit: guarded candidate wins when Condition() is true", "[stage05][earlyexit]") {
    auto token = ModelToken<S05DomainRules>::FromType<S05UnitRules>();
    auto gate  = CapabilityRouter<S05DomainRules>::Find<IFireControl>(
                     token, IFireControl::RuleContext{7});

    REQUIRE(gate);
    IFireControl::Params p;
    gate(p);
    REQUIRE(p.m_ShouldFire == true);
}

TEST_CASE("Stage 05 — EarlyExit: falls through to the unconditional candidate", "[stage05][earlyexit]") {
    auto token = ModelToken<S05DomainRules>::FromType<S05UnitRules>();
    auto gate  = CapabilityRouter<S05DomainRules>::Find<IFireControl>(
                     token, IFireControl::RuleContext{2});

    REQUIRE(gate);
    IFireControl::Params p;
    gate(p);
    REQUIRE(p.m_ShouldFire == false);
}
