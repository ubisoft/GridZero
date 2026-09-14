# chain_splicer — Design Notes

## Purpose

`ChainSplicer<TNode>` is the single source of truth for the cross-binary intrusive-chain splice protocol used by every plugin synchronizer (the "Mass Transfer" pattern). `DomainSynchronizer` and `TaskSynchronizer` both build on top of this one struct template.

One struct template, one splice path, one unsplice path. Adding a new `TNode` type costs zero policy code.

## Vocabulary

- **OpenPortal** — call `SetRemoteTarget(host)` on the anchor pair so the plugin's `Get()` resolves into host-owned storage.
- **Splice** — append the host's pre-splice chain behind the plugin's local tail and re-point the host anchor at the plugin head.
- **Unsplice** — symmetric reversal: rebuild the original two chains. Cross-binary redirects are intentionally left open so callers can refresh derived caches into the host's storage before closing.
- **ClosePortal** — call `SetRemoteTarget(nullptr)` so the plugin's `Get()` resolves locally again. Must be issued **after** any post-unsplice cache rebuild so the rebuild's writes still land in the host's storage.

## Correct sequence (plugin unload)

1. `Unsplice()` — restores host chain topology, redirects remain open.
2. Refresh derived caches (writes go into host storage via the open redirect).
3. `ClosePortal()` — closes redirects, resets splice state.

## Contract on TNode

- Derives from `NodeLink<TNode, _>` (i.e. has `TNode* m_Next`).
- Has a registered `NodeLinkAnchor` + `CacheAnchor` (`UniversalAnchor` pair).
- `SecurePluginCoupler<TNode>` compiles for the type.

## Splice state

`ChainSplicer` lives in plugin storage at static-init time. Every plugin's `CRG_Bootstrap` and `CRG_Shutdown` run inside the same DLL, so the state read at unload is always the state captured at load.
