# domain_synchronizer — Architecture Notes

## Purpose

`DomainSynchronizer<TDomain>` is the per-domain user hook that fires when a
plugin's chain topology changes. It is forward-declared in
`plugin_chain_synchronizer.hpp`; `CRG_DEFINE_DOMAIN(D)` emits the
specialization with the canonical CRG actions:

- **OnPluginLoad / OnPluginUnload** — refresh `DomainArenaPopulator`'s cache
  and the per-domain `CapabilityRouter` dispatch table (host's storage,
  redirected via the still-open cross-binary anchor).

User code can override the macro body when a domain needs additional reactions
(custom indices, observers, etc.).

## Idempotency requirement

The hook fires **once per chain that actually changed topology**. Multiple
chain agents on the same domain may invoke it repeatedly within one
Load/Unload, so the body must be idempotent. `RefreshCache` and `MarkDirty`
both satisfy this.

## Agent wiring

The `ISynchronizer` wired into the plugin pipeline is
`PluginChainSynchronizer<TDomain, DomainArenaPopulator>`, held as
`internal::SyncAgentHolder<TDomain>::Instance`. It owns the populator chain's
splice protocol and is auto-registered as part of the plugin's local sync chain.
