# Stage 05 - Dynamic Rules: EarlyExit Dispatch Cell

## Problem
Several candidate capabilities can serve the same cell, but only one should run — the choice
depends on **runtime data** (a `RuleContext`), not on the compile-time model/axis coordinate
alone.

## Solution - the default `DispatchCell` (EarlyExit)

```cpp
// Contract: Params + a RuleContext type
struct IFireControl {
    struct Params      { bool m_ShouldFire{ false }; };
    struct RuleContext { int m_ThreatLevel{ 0 }; };
};

// Config: a Condition(RuleContext) predicate
struct ThreatConfig {
    int m_MinThreat{ 0 };
    bool Condition(const IFireControl::RuleContext& ctx) const {
        return ctx.m_ThreatLevel >= m_MinThreat;
    }
};

// Guarded candidate: carries a Config
template<typename TModel>
struct AggressiveFire : public Capability<IFireControl, ThreatConfig> {
    AggressiveFire() { this->m_Config.m_MinThreat = 5; }
    static void Execute(IFireControl::Params& p) { p.m_ShouldFire = true; }
};

// Fallback candidate: no Config -> always matches
template<typename TModel>
struct PassiveFire : public Capability<IFireControl> {
    static void Execute(IFireControl::Params& p) { p.m_ShouldFire = false; }
};

// Binding order is the scan order: guarded first, fallback last
namespace { static const CapabilityBinding<MyDomain, MyUnit, AggressiveFire, PassiveFire> s_b; }

// Find() takes the RuleContext as an extra argument
auto gate = CapabilityRouter<MyDomain>::Find<IFireControl>(handle, IFireControl::RuleContext{7});
```

`Find()` walks the bound candidates for the cell in binding order and returns the first whose
`Condition(ctx)` is true — "early exit", not a full scan-and-score. A candidate with no `Config`
(no `Condition`) always matches, so it works as a fallback/default entry when placed last.

## Other dispatch-cell strategies live elsewhere
This stage keeps only the default `DispatchCell` (EarlyExit) — the shape every prior stage
already relies on implicitly. The alternative strategies are covered separately:
- `UtilityScoringCell` (highest-score-wins, injected via `CapabilityRoutingTraits::DispatchCellType`)
  and a `RuleContext` combined with an N-D routing axis: `routing/dispatch_cell_strategies.test.cpp`.
- The guarantee that `Find()` never copies the `RuleContext` argument: `routing/rule_context.test.cpp`.

## What's new vs Stage 04
`Find()` gains a `RuleContext` argument. Which bound candidate runs is now a runtime decision
(`Condition(ctx)`), not fixed by the model/axis coordinate alone.
