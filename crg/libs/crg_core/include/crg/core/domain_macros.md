# domain_macros — Design Notes

## Capability Domain vs Pipeline Domain

`CRG_DECLARE_CAPABILITY_DOMAIN(TDomain)` registers a type as a Capability Domain: it owns a tensor of capability dispatch cells (Muscle/Brain routing) but does not schedule tasks.

For domains that also schedule tasks via a TaskGraph, use `CRG_DECLARE_PIPELINE_DOMAIN` instead. Pipeline domains compose the capability tensor with a TaskNode anchor, DomainGraph, and DomainState.

The corresponding define macros follow the same split:
- `CRG_DEFINE_CAPABILITY_DOMAIN` — emits the `DomainSynchronizer` specialization that refreshes the populator cache and capability router dispatch table.
- `CRG_DEFINE_PIPELINE_DOMAIN` — composes the above with task-graph synchronization.

## Legacy aliases

`CRG_DECLARE_DOMAIN` and `CRG_DEFINE_DOMAIN` are aliases for the capability variants, kept for migration. New code should use the explicit `_CAPABILITY_` or `_PIPELINE_` forms.

## Removed macros

`CRG_BEGIN_DOMAIN` / `CRG_END_DOMAIN` were removed in favor of the variadic `CRG_DECLARE_DOMAIN_MODELS(TDomain, M1, M2, ...)` macro defined in `crg/models/model_key.hpp`. The new form is single-statement, carries the same `DomainKey` static_assert, and additionally specializes `ModelDomain<M>` for each model — required by `PipelineCapability<TModel, At<Phase>>`.

## Internal macro: CRG_INSTANTIATE_CAPABILITY_DOMAIN_ANCHORS

When `CRG_PLUGINS_ENABLED`, this macro emits four explicit instantiations per binary (host + each plugin):

1. `SyncAgentHolder<TDomain>` — holds the per-domain sync agent static members.
2. `UniversalAnchor<DomainArenaPopulator<TDomain>*>` — single-pointer anchor (remote target).
3. `UniversalAnchor<std::vector<...>>` — vector anchor (all registered populators).
4. `DomainAnchorRegistry<TDomain>` — whose `static inline UniversalAnchor<...>` members construct one instance per binary. These constructors run `NodeLink::NodeLink()` and populate `internal::PluginLocalStorage<AnchorLink>::s_LocalHead`. Without this instantiation the AnchorLink chain stays empty in every binary and Mass Transfer silently degrades (cache.size() == 1 forever).

When `CRG_PLUGINS_ENABLED` is false, both macros expand to nothing (monolithic build, no plugin topology to synchronize).

## DomainSynchronizer hook

`DomainSynchronizer<TDomain>` (forward-declared in `plugin_chain_synchronizer.hpp`) is fired by every chain agent that detects a real topology change. For capability-only domains the populator is the only such agent; for pipeline domains the task-graph agent also fires the hook.

The refresh order inside `OnPluginLoad` is fixed: `DenseIndexStore` first, then `DomainArenaPopulator`, then `CapabilityRouter`. `CapabilityBinding::Populate` requires its dense slot to already exist when it runs.
