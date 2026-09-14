# plugin_context — design notes

## Lifecycle ownership

`CRG_Context` is owned by the host's `PluginLoader` instance, never by a global. Being value-typed, two `PluginLoader` instances can coexist in the same process without trampling each other's state.

## No cleanup-action table

There is no table of cleanup actions in `CRG_Context`. Each `ISynchronizer` carries its own `ChainSplicer<TNode>` static-inline (one per binary). The splicer is captured during `OnPluginLoad` and consumed during `OnPluginUnload`. See `discovery/chain_splicer.hpp` for the mechanism.
