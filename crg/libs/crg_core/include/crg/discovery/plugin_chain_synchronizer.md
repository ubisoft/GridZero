# plugin_chain_synchronizer — Design Notes

## Purpose

`PluginChainSynchronizer<TDomain, TNode>` is a generic `ISynchronizer` that owns
the load/unload protocol for one intrusive `NodeLink` chain that spans the
plugin/host binary boundary.

## Protocol split

The pointer plumbing (Splice / Unsplice / ClosePortal) is centralized in
`chain_splicer.hpp` — a single source of truth for the cross-binary splicing
protocol.

## Per-domain user hook

The post-change reaction is centralized per *domain* (not per chain type) via
`DomainSynchronizer<TDomain>::OnPluginLoad/OnPluginUnload`.

`DomainSynchronizer` is **forward-declared only**. Forgetting to specialize it
via `CRG_DEFINE_DOMAIN` surfaces as a compile error ("use of incomplete type")
at the point of instantiation — never as a deferred linker error (LNK2019).

The hook may be called more than once per plugin load when several chains splice
in the same load wave; implementations should be idempotent (e.g., RefreshCache,
MarkDirty both are).

## Extension cost

Adding a new synchronized chain requires one `PluginChainSynchronizer` alias.
The per-domain hook (the one function the user writes) remains unchanged.

## Ordering invariant in OnPluginUnload

The cross-binary redirect must remain open for the duration of
`DomainSynchronizer<TDomain>::OnPluginUnload` so that any cache writes performed
by the hook land in the **host's** storage, not the plugin's (which is about to
be unloaded). `ClosePortal` severs the redirect only after the hook returns.
