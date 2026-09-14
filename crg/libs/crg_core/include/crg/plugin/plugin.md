# plugin.hpp — Design Notes

## Bootstrap / Shutdown protocol

`CRG_DEFINE_PLUGIN(PluginName)` emits four C-ABI entry points:

- `CRG_Bootstrap_<Name>(CRG_Context*)` — plugin-side bootstrap: fires `OnPluginPreLoad` on all local `ISyncNode`s, walks the plugin's `AnchorLink` list, resolves each anchor against the host's hash directory, then fires `OnPluginLoad`.
- `CRG_Shutdown_<Name>()` — plugin-side teardown: fires `OnPluginUnload` on all local `ISyncNode`s.
- `CRG_Bootstrap(ctx)` / `CRG_Shutdown()` — generic aliases that delegate to the named variants.

Bootstrap and Shutdown are **symmetric**: they expand to the same template instantiations and use the same static-inline storage, so state captured during bootstrap is correctly released during shutdown.

## Cross-binary anchor and bit-stability

`CRG_IMPLEMENT_PARTITION(TDomain)` defines the cross-binary anchor for the per-domain hash-to-slot directory.

The anchor key uses the same XOR-mix TypeHash that the legacy `DenseIDStore<TDomain>` used. This is intentional: after the rename to `DenseIndexState<TDomain>`, the hash must remain bit-for-bit identical to preserve compatibility with already-compiled plugins and serialised state. Do not change the hash formula without a coordinated rebuild of all consumers.
