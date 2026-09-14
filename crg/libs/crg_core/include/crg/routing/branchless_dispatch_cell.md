# BranchlessDispatchCell — Design Notes

## Purpose

An alternative `DispatchCell` implementation using Structure-of-Arrays (SoA) layout,
bitmask evaluation, and TZCNT (count trailing zeros) for O(1) winner selection.
Injected per domain+contract pair via `CapabilityRoutingTraits::DispatchCellType`.

## API contract

- **Hot path**: `EvaluateAndExecute(ctx, params)` — combined routing and execution in one call; no Find/gate split.
- **Cold path**: `Bind<Impl>(target, implPtr)` — called once per capability at arena build time.
- **Compat shim**: `Resolve(ctx)` — wraps the winning execute function pointer in a `CapabilityHandle` so the standard two-step `Find → gate(params)` API still works alongside `EvaluateAndExecute`.

## Capacity and priority

Supports up to 64 dynamic rules. Rule index 0 is highest priority: the bitmask evaluation
sets one bit per matching rule, then TZCNT picks the lowest set bit (first rule that matched).

## CountTrailingZeros64

Pure C++17 portable 6-step binary search. Clang 6+, GCC 7+, and MSVC 19.20+ idiom-recognize
this pattern and emit `BSF`/`TZCNT` without any compiler-specific intrinsic. Precondition:
`mask != 0` — callers must guard before invoking.

## TConfig stateless requirement

`Bind<Impl>` captures the condition as a lambda converted to a plain function pointer
(`CondFn = bool(*)(const ContextType&)`). This requires `Impl::ConfigType` to be
default-constructible and fully stateless (all constraint values are `static constexpr`
fields). The lambda stores no captures; a single `static const` instance is used inside.
