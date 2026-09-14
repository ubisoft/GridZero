# Stage plugin_loadlibrary — Manual-Load Plugin

## Purpose

Demonstrates the CppCon Acte 1 pattern: loading a capabilities plugin at runtime
using the consumer's own `LoadLibrary`/`dlopen`, with zero GridZero `.cpp` in either
binary. The plugin injects `IGreet` capabilities for `English` and `French` models
into `GreeterDomain`. The host invokes them, then unloads cleanly.

## What this proves

- Zero GridZero `.cpp` in the host — `crg_core` is header-only.
- Zero GridZero `.cpp` in the plugin — only `crg_core` headers.
- The host writes one `LoadLibrary` + two `GetProcAddress` calls. That is the full
  integration surface.
- After `CRG_Shutdown` + `dlclose`, the routing tensor is identical to the pre-load state.
- `CRG_DEFINE_PLUGIN(Greeter)` generates the C ABI entry points in the plugin DLL.
  `CRG_DEFINE_DOMAIN(GreeterDomain)` on both sides wires the `DomainSynchronizer` and
  `DomainAnchorRegistry` needed for the chain splice.

## Load sequence

1. Host builds `HostDescriptor{m_HostAnchorsHead = AnchorLink::GetLocalHead()}` and
   `CRG_Context{&hostDesc}`.
2. Host calls `OpenDylib(CRG_GREETER_PLUGIN_PATH)` — its own code, no CRG loader.
3. Host resolves `CRG_Bootstrap` via `GetSym` and calls it with the context.
   - Plugin's `ISyncNode::VisitLocal` visits `SyncAgentHolder<GreeterDomain>::Instance`.
   - `PluginChainSynchronizer::OnPluginLoad` calls `ChainSplicer::Splice`: plugin
     anchors are spliced in front of host anchors; `DomainSynchronizer::OnPluginLoad`
     refreshes `DenseIndexStore`, `DomainArenaPopulator`, and `CapabilityRouter`.
4. `CapabilityRouter<GreeterDomain>::Find<IGreet>` now resolves the plugin capabilities.
5. Host calls `CRG_Shutdown` + `CloseDylib`.
   - `Unsplice` reverses the splice; `ClosePortal` severs cross-binary redirects.
   - `DomainSynchronizer::OnPluginUnload` refreshes caches again.

## Files

| File | Role |
|---|---|
| `plugin_greeter.cpp` | Plugin DLL: bindings + `CRG_DEFINE_PLUGIN(Greeter)` |
| `stage_plugin_loadlibrary.test.cpp` | Host: `LoadLibrary` + invoke + unload |
| `CMakeLists.txt` | `crg_greeter_plugin` (SHARED) + `Run_PluginLoadLibrary_Tests` |

## Dependencies

- `crg_core` (header-only, no `.cpp`)
- `CRG_PLUGINS_ENABLED=1` on both targets
- `CRG_GREETER_PLUGIN_PATH` injected by CMake via generator expression
