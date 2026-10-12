# Stage plugin_loadlibrary — Manual-Load Plugin

## Purpose

Demonstrates the CppCon Acte 1 pattern: loading a capabilities plugin at runtime using the consumer's own `LoadLibrary`/`dlopen`, with zero GridZero `.cpp` in either binary. The plugin injects `IGreet` capabilities for `English` and `French` models into `GreeterDomain`. The host invokes them, then unloads cleanly.

## What this proves

- Zero GridZero `.cpp` in the host — `crg_core` is header-only.
- Zero GridZero `.cpp` in the plugin — only `crg_core` headers.
- The host writes one `LoadLibrary` + two `GetProcAddress` calls. That is the full integration surface.
- After `CRG_Shutdown` + `dlclose`, the routing tensor is identical to the pre-load state.
- `CRG_DEFINE_PLUGIN(Greeter)` generates the C ABI entry points in the plugin DLL. `CRG_DEFINE_DOMAIN(GreeterDomain)` on both sides wires the `DomainSynchronizer` and `DomainAnchorRegistry` needed for the chain splice.

## The Load Sequence

### 1. The Developer Experience (What you write)
1. **Setup**: The host builds a `HostDescriptor` and a `CRG_Context`.
2. **Load**: The host calls `OpenDylib(CRG_GREETER_PLUGIN_PATH)`.
3. **Bootstrap**: The host resolves `CRG_Bootstrap` via `GetProcAddress`/`dlsym` and calls it with the context.
4. **Execute**: The capabilities are now available. `CapabilityRouter::Find` naturally resolves the plugin's targets.
5. **Shutdown**: The host resolves and calls `CRG_Shutdown`, then unloads the binary via `CloseDylib`.

### 2. Under the Hood (What GridZero does)
* When `CRG_Bootstrap` is called, the plugin's local AnchorLink chain is actively **spliced** into the host's anchors without requiring any centralized registry.
* `DomainSynchronizer::OnPluginLoad` automatically rebuilds the `CapabilityRouter` tensor caches.
* When `CRG_Shutdown` is called, the splice is cleanly reversed and cross-binary redirects are severed, perfectly restoring the host's memory state.

## Build System & Isolation

To properly configure GridZero plugin boundaries, the build system must enforce strict memory isolation.

* **CMake Configuration**: Use the provided `crg_add_plugin` and `crg_add_host` functions (located in `cmake/crg_plugin_api.cmake`) to automatically configure platform-specific dynamic library export definitions and link against `crg_core`.
* **CRG Activation**: Both host and plugin targets must be compiled with the `CRG_PLUGINS_ENABLED=1` definition.
* **Strict Visibility**: For custom build systems, you must force strict isolation for plugin template statics and inline variables (`CXX_VISIBILITY_PRESET hidden` and `VISIBILITY_INLINES_HIDDEN ON` for CMake, or their compiler equivalents).

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