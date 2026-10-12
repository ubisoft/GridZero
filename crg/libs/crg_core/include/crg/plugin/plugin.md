# plugin.hpp — Design Notes

## Bootstrap / Shutdown protocol

`CRG_DEFINE_PLUGIN(PluginName)` emits four C-ABI entry points:

- `CRG_Bootstrap_<Name>(CRG_Context*)` — plugin-side bootstrap (two-phase load): 
  1. Fires `BindStorage` on all local `ISyncNode`s to establish memory bridges (resolving raw anchors and vectors against the host's hash directory).
  2. Fires `SpliceTopology` to insert nodes into the active chains and populate the shared memory caches.
- `CRG_Shutdown_<Name>()` — plugin-side teardown (two-phase unload): 
  1. Fires `UnspliceTopology` on all local `ISyncNode`s to purge arena data and safely sever the chain links while the memory bridges are still active.
  2. Fires `UnbindStorage` to reset pointers and cut the physical memory anchors.
- `CRG_Bootstrap(ctx)` / `CRG_Shutdown()` — generic aliases that delegate to the named variants.

Bootstrap and Shutdown are **symmetric**: they expand to the same template instantiations and use the same static-inline storage, ensuring that state captured during bootstrap is correctly and safely released during shutdown in the proper logical order.

## Cross-binary anchor and bit-stability

`CRG_DECLARE_DOMAIN(TDomain)` and `CRG_DEFINE_DOMAIN(TDomain)` declare the domain and define its cross-binary anchor for the per-domain hash-to-slot directory (`DenseIndexState<TDomain>`).

The anchor key uses the same XOR-mix TypeHash that the legacy `DenseIDStore<TDomain>` used. This is intentional: after the rename to `DenseIndexState<TDomain>`, the hash must remain bit-for-bit identical to preserve compatibility with already-compiled plugins and serialised state. Do not change the hash formula without a coordinated rebuild of all consumers.