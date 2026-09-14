# dispatch_strategies — Design Notes

## Purpose

This file provides built-in alternative dispatch cell types to replace the default `DispatchCell` (early-exit, defined in `tensor_arena.hpp`). The active cell type for a given domain+contract pair is selected via the `DispatchCellType` alias in `CapabilityRoutingTraits<TDomain, TContract>`.

## Available strategies

### DispatchCell (default, in tensor_arena.hpp) — Early-Exit

Evaluates rules in priority order. The first rule whose `bool Condition(const RuleContext&) const` returns `true` wins.

### UtilityScoringCell (this file) — Utility Scoring

Evaluates all registered scorers and picks the one with the highest float score. Scorers must expose `float Evaluate(const RuleContext&) const` on their config type. Falls back to the static baked capability (no config) if no scorers are registered or all scores are `-infinity`.

## Selecting a strategy

```cpp
namespace crg::routing {
    template<> struct CapabilityRoutingTraits<MyDomain, ICombatAI> {
        using SpaceType        = CapabilitySpace<>;
        using DispatchCellType = UtilityScoringCell<MyDomain, ICombatAI>;
    };
}
```

The `DispatchCellType` alias is injected per domain+contract; omitting it defaults to `DispatchCell`.
