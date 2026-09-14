# capability_routing_traits — design notes

## Purpose

Provides compile-time machinery to resolve the `RuleContext` type for a `(TDomain, TContract)` pair. The result is exposed as the alias `ContextTypeOf<TDomain, TContract>`.

## Context resolution priority

1. `CapabilityRoutingTraits<TDomain, TContract>::RuleContext` — explicit override at the routing-traits specialization level (highest priority).
2. `TContract::RuleContext` — type defined directly on the contract (fallback).
3. `NullContext` — synthesized default used when neither of the above is present.

`FullContext<TDomain, TContract>` (bool-specialized on `HasDynamicRules`, mirrors `CapabilityHandle<TContract, bool>`) exposes `Base` - the exact context type `Find()` operates on: the contract's `RuleContext` when dynamic rules are in play, or `NullContext` otherwise - and `RequiresContext`, the bool `Find()`'s two overloads are SFINAE'd on. The rule-bearing overload takes its `RuleContext` argument as `const FullContext<TDomain, TContract>::Base&` and forwards it straight into `DispatchCell::Resolve()`; no copy is made inside `Find()`. The other overload never touches caller arguments to build a context at all - it just default-constructs a `NullContext`.

## Why lazy extraction

`std::conditional_t<B, T, F>` eagerly instantiates both branches. Accessing `T::RuleContext` when `T` has none would be a hard error even in the un-selected branch. `ExtractRuleContext` wraps the member access in a `std::void_t` specialization so the member is only accessed when it exists.

## Default SpaceType

`CapabilityRoutingTraits` defaults `SpaceType` to `CapabilitySpace<>` (0-dimensional, volume 1). Specializations that need N-D routing override `SpaceType` with `CapabilitySpace<Axis1, Axis2, ...>`.
