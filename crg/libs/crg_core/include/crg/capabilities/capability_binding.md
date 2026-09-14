# capability_binding — Design Notes

## Purpose

`CapabilityBinding<TDomain, TModel, TCapabilities...>` is the glue that, at static-init time, registers a model's capability implementations into their domain's tensor arenas. It derives from `DomainArenaPopulator<TDomain>` and is auto-discovered via `NodeLink`.

## CapabilityNode — disambiguation cast

`CapabilityNode` inherits multiply from every `TCap<TModel, At<Space, I>>` specialisation across the space volume. When there are multiple specialisations that share the same Brain contract (polymorphic), a naive `CapabilityNode* → Contract*` cast is ambiguous. The fix is to pre-cast to the concrete `Impl*` first (`static_cast<const Impl*>(this)`), then let `BindingTarget::SetTarget<Impl>` perform the unambiguous upcast.

## DispatchCell — pluggable binding and resolution strategy

`cell.Bind<Impl>(target, implPtr)` delegates to whatever cell type is injected via `CapabilityRoutingTraits<TDomain, Contract>::DispatchCellType`. The default `DispatchCell` implements an Early-Exit bool-predicate strategy. Custom cell types (e.g. UtilityScoring) plug in here and own their own binding and resolution logic.

## Bootstrap ordering protocol

`DenseIndex::Refresh()` is called at the top of `Populate()` to ensure the `DenseIndexStore<TDomain>` is up to date before any arena allocation.

- **Normal operation**: `DomainSynchronizer` calls `Refresh` on every plugin load/unload cycle, so by the time `Populate` runs the store is already current.
- **First bootstrap**: No plugin-load event has fired yet; `Refresh` is called lazily on the first `Populate` of the domain (see `DenseIndexStore::Refresh` contract).

If `DenseIndex::Find(Hash)` returns `InvalidDenseSlot`, the model was declared via `CRG_DECLARE_DOMAIN_MODELS` (the `static_assert` above passed) but its `ModelNode` never reached the chain that `Refresh` walks. Likely causes:
- Missing `CRG_DEFINE_DOMAIN` in a plugin TU.
- The binding TU links before its model-declaration TU, so the `ModelNode` static is not yet constructed when `Refresh` runs.

## DOD contract enforcement

For non-polymorphic (DOD/Muscle) contracts, two `static_assert`s fire at `CapabilityNode` instantiation:
1. The contract must declare a nested `Params` struct (`IsStaticContract<Contract>`).
2. The implementation must provide `static void Execute(Params&)` (`HasStaticExecute<Impl, Params>`).

These are caught at class instantiation — before any arena allocation or chain walk.
