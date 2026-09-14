# GridZero - Capability Routing Grid (CRG)

> Companion code to "Capability Routing Grid - From Decoupled Plugins to the
> Hardware Ceiling", Cyril Tissier's talk at CppCon 2026. Slides and recording
> will be linked here once available.

GridZero is a C++17 header-only toolkit for composing decoupled code across
strictly segregated projects. `NodeLink` lets implementations register
themselves - even across DLL boundaries - with no central registry and no
`Init()` call; that alone is enough to build a plugin system, no routing
tensor required. The capability router covers the other pole: bind behavior to
model types through a flat tensor lookup instead of a vtable, and for a
contract with no virtual methods that same lookup costs zero virtual calls and
zero heap allocation - decoupled plugins to the hardware ceiling, as the talk
title has it.

## What is in this repository

This is the **public tier** of GridZero. It contains:

- **`crg_core`** - the complete routing engine (header-only, ~80 headers).
- **Test stages** - eight executable stages (00-07) that build up the framework
  layer by layer, plus the `plugin_loadlibrary` stage demonstrating manual-load
  plugins.

There is no managed loader, no hot-reload pipeline, no JIT compiler, and no
WASM backend target in this repository - those live in a separate protected tier.

## Quick build

```bash
cmake -S gridzero -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/crg/tests/core_monolith/Run_CoreMonolith_Tests
./build/crg/tests/plugin_loadlibrary/Run_PluginLoadLibrary_Tests
```

Requires a C++17 compiler. No external dependencies for `crg_core`.

## The four layers

| Layer | Headers | Concept |
|---|---|---|
| Discovery | `crg/discovery/` | NodeLink self-registration, plugin splice |
| Models | `crg/models/`, `crg/core/` | DenseID, domain, TypeHash |
| Capabilities | `crg/capabilities/`, `crg/routing/` | Tensor routing, DOD/Brain dispatch |
| Type erasure | `crg/type_erasure/` | ModelShell, Invoke/TryInvoke |

See `architecture.md` for the full design rationale.

## Test stages

Each stage is a self-contained `.test.cpp` + `.md` pair that introduces one concept:

| Stage | Concept |
|---|---|
| 00 | NodeLink: linker-driven discovery |
| 01 | DenseID, ModelToken, domain |
| 02 | Muscle capability: DOD path, 0-D tensor, ~1 ns |
| 03 | Contextual routing: 1-D axis |
| 04 | N-D routing: Horner branchless offset |
| 05 | Brain capability: virtual interface, `operator->` |
| 06 | Dynamic rules: EarlyExit, UtilityScoring |
| 07 | ModelShell: type erasure + Invoke/TryInvoke |
| plugin_loadlibrary | Manual-load plugin: `LoadLibrary` + `CRG_Bootstrap` |

## License

Apache License 2.0 - see `LICENSE` (added at publication time).
