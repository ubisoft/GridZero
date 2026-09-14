# UniversalAnchor — Architecture Notes

## Purpose

`UniversalAnchor<T>` provides a stable per-type memory address for registries so that different binaries sharing a process can agree on a single canonical head pointer for each registry type.

## Two Modes

### Monolithic Mode (`CRG_PLUGINS_ENABLED` not defined)

The anchor resolves to a simple static-local instance inside the current TU. Everything lives in one binary, so no cross-binary synchronisation is needed. `CRG_DEFINE_UNIVERSAL_ANCHOR(T)` expands to nothing because the anchor is located on demand.

### Plugin (DLL) Mode (`CRG_PLUGINS_ENABLED`)

Multiple binaries (the host executable and loaded plugins) each have their own copy of static storage. Without coordination they would each see their own empty registry head, making them invisible to one another.

The solution is the **Mass Transfer** pattern:

1. Each plugin binary contains one `UniversalAnchor<T>` instance per anchor type, placed in that binary's data section via an explicit template instantiation (`template struct UniversalAnchor<T>`).
2. A companion static-const variable forces `NodeLink::NodeLink()` to run at load time, self-registering the anchor into `internal::PluginLocalStorage<AnchorLink>::s_LocalHead` for that binary.
3. During plugin bootstrap, the host walks every binary's local `AnchorLink` chain and merges them into the single shared head pointer — the "mass transfer" step.

`CRG_DEFINE_UNIVERSAL_ANCHOR(T)` must be placed in exactly one `.cpp` per plugin binary for each type `T` that crosses the binary boundary.

## Portation Macros

- `CRG_DECLARE_UNIVERSAL_NODE_ANCHOR(T)` — registers hash aliases for `T*` and `std::vector<const T*>` so the type-erased routing tables can look them up by stable hash strings.
- `CRG_INTERNAL_DECLARE_UNIVERSAL_DOMAIN_ANCHOR(TDomain)` — convenience wrapper that applies the above to the internal `DomainArenaPopulator<TDomain>` type used by domain bootstrap.
