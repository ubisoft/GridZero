# plugin_local_node — Design Notes

`PluginLocalNode<TNode, TContract>` is a specialization of `NodeLink` that adds a
plugin-local registration list (`GetLocalHead` / `VisitLocal`).

**Why a separate base type**

Local-head storage relies on `CRG_MARK_INTERFACE_BOOTSTRAP`, the plugin splice
mechanism that wires a per-plugin static list at load time. This splice has no
meaning in a monolithic build, so `PluginLocalNode` is only defined when
`CRG_PLUGINS_ENABLED` is set. Plugin-local node types must derive from
`PluginLocalNode` instead of `NodeLink` directly to gain this local-list capability.
