# tensor_arena — Architecture Notes

## Purpose

`TensorArena<TDomain, TContract>` is the physical memory backing for the capability routing system. Each `(TDomain, TContract)` pair owns one arena — a flat `std::vector` of dispatch cells shared across EXE/DLL boundaries via `UniversalAnchor`.

## Key types

- **`DynamicRule`** — A conditional capability binding: a predicate function pointer, opaque config data, and the target descriptor. Evaluated in O(K) before the static fallback. No built-in priority field — ordering is the cell's responsibility.
- **`DispatchCell`** — The default dispatch cell implementing the Early-Exit strategy: scan dynamic rules in insertion order; first match wins; fall back to the statically baked capability if none match. Replaceable per domain+contract via `CapabilityRoutingTraits::DispatchCellType`.
- **`CellSelector`** — Trait that picks `DispatchCell` by default, or a user-supplied cell type if `CapabilityRoutingTraits<TDomain, TContract>::DispatchCellType` is defined.
- **`TensorArenaStorage`** — Plain struct wrapping the cell vector; exists as a separate type so `UniversalAnchor` can give it a stable identity across module boundaries.
- **`TensorArena`** — Facade with public read-only accessors (`GetData()`, `GetSize()`) for the hot-path `CapabilityRouter`, and a private mutable accessor (`Get()`) gated by friend declarations to `CapabilityNode` and `CapabilityBinding` (cold-path writers only).

## Access control intent

`CapabilityRouter` is deliberately **not** a friend of `TensorArena`. It reads the arena only through `GetData()`/`GetSize()`, preventing any write access on the hot path. Only `CapabilityNode` and `CapabilityBinding` (cold-path, build-time) can mutate the cell vector.

## Dynamic rule ordering

`DynamicRule` carries no `m_Priority` field. Ordering strategy belongs to the cell, not the framework. Two mechanisms:

1. **Insertion order** (default `DispatchCell`): rules arrive in `Populate()` order, which is controlled by `NodeLinkTraits<TPopulator>::SortCache` at the chain level.
2. **Custom cell** (via `DispatchCellType`): override `Bind()` to sort `m_DynamicRules` by any criterion — an int priority in your config, a float score, a string key. The `UtilityScoringCell` built into CRG is one example (highest float score wins instead of first match).

## Custom dispatch cell

Override the default Early-Exit cell per domain+contract:

```cpp
namespace crg::routing {
    template<> struct CapabilityRoutingTraits<MyDomain, ICombatAI> {
        using SpaceType        = CapabilitySpace<>;
        using DispatchCellType = UtilityScoringCell<MyDomain, ICombatAI>;
    };
}
```

## Auto-hashing

`TensorArenaStorage` gets a stable `TypeHash` derived from the XOR/Fibonacci-mix of `TypeHash<TDomain>` and `TypeHash<TContract>`. This is what `UniversalAnchor` uses to locate the shared singleton across module boundaries.
