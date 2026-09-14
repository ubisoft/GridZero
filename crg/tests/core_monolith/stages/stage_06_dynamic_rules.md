# Stage 06 - Specializable Dispatch Cells: EarlyExit & UtilityScoring

## Problem
Dynamic rule evaluation isn't uniform:
- LiveOps overrides need a bool predicate with priority (first match wins).
- AI behavior selection needs a float score (highest score wins).
Hard-coding a single strategy into the tensor cell blocks the other one.

## Solution - a specializable `DispatchCell`

`CellSelector<TDomain, TContract>` picks the cell type for each slot:
- **Default** -> `DispatchCell` (Early-Exit: bool predicate, priority order)
- **Override** -> any cell injected via `CapabilityRoutingTraits::DispatchCellType`

### Cell contract (two methods)
```cpp
Bind<Impl>(target, instance)   // called once when the arena is built
Resolve(ctx)                   // called on every Find()
```

## Part A - EarlyExit (default)

```cpp
// Config with a bool predicate + priority
struct ThreatConfig {
    int m_Priority{ 0 };
    int m_MinThreat{ 0 };
    bool Condition(const IFireControl::RuleContext& ctx) const {
        return ctx.m_ThreatLevel >= m_MinThreat;
    }
};

// Dynamic capability with config
template<typename TModel, typename TAt>
struct AggressiveFire : public Capability<IFireControl, ThreatConfig> {
    AggressiveFire() {
        this->m_Config.m_Priority  = 10;
        this->m_Config.m_MinThreat = 5;
    }
    static void Execute(IFireControl::Params& p) { p.m_ShouldFire = true; }
};

// Static fallback (no config -> always evaluated last)
template<typename TModel, typename TAt>
struct PassiveFire : public Capability<IFireControl> {
    static void Execute(IFireControl::Params& p) { p.m_ShouldFire = false; }
};

// RuleContext passed to Find() - evaluated by each Condition()
auto gate = CapabilityRouter<Domain>::Find<IFireControl>(
                handle, IFireControl::RuleContext{7});
```

## Part B - UtilityScoring (injected)

```cpp
// Inject the scoring strategy for this domain+contract
namespace crg::routing {
    template<> struct CapabilityRoutingTraits<MyDomain, ICombatAI> {
        using SpaceType        = CapabilitySpace<>;
        using DispatchCellType = UtilityScoringCell<MyDomain, ICombatAI>;
    };
}

// Scoring config: float Evaluate() instead of bool Condition()
struct AttackScorer {
    float Evaluate(const ICombatAI::RuleContext& ctx) const {
        return ctx.m_ThreatLevel * 0.8f;
    }
};
```

## Compile-time verification
```cpp
// The selected cell type is statically checkable
using Selected = CellSelector<MyDomain, ICombatAI>::Type;
static_assert(std::is_same_v<Selected, UtilityScoringCell<MyDomain, ICombatAI>>);
```

## What's new vs Stages 02-05
`DispatchCellType` in the routing traits. Two cell strategies in the same test.
`FillArena` no longer writes `m_DynamicRules` directly - it calls `cell.Bind`.
`Find` no longer iterates `m_DynamicRules` directly - it calls `cell.Resolve`.

## Part C - combined N-D axes + RuleContext

`Find<TContract>` resolves to one of two SFINAE'd overloads based on
`FullContext<TDomain, TContract>::RequiresContext`. A contract that has both
routing axes (`SpaceType` with `Dimensions > 0`) and a `RuleContext` calls the
rule-bearing overload, with the axes passed after the `RuleContext`:

```cpp
namespace crg::routing {
    template<> struct CapabilityRoutingTraits<MyDomain, IPatrol> {
        using SpaceType = CapabilitySpace<Terrain>;
    };
}

auto gate = CapabilityRouter<MyDomain>::Find<IPatrol>(
                handle, IPatrol::RuleContext{7}, Terrain::Flat);
```

Bound capabilities take `template<typename TModel, typename TAt>` (two
parameters), matching Stage 04's `Dimensions > 0` convention, since the arity
is driven by `SpaceType::Dimensions` and is independent of `RuleContext`.

## Part D - RuleContext is never copied by Find()

`Find()`'s rule-bearing overload takes `const FullContext<TDomain, TContract>::Base&`
and forwards it straight into `DispatchCell::Resolve()`. Part D binds a
copy-counting `RuleContext` and asserts the count stays at zero across a call.
