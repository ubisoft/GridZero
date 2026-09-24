# GridZero - Capability Routing Grid (CRG)

> Companion code to "Capability Routing Grid - From Decoupled Plugins to the
> Hardware Ceiling", Cyril Tissier's talk at CppCon 2026. Recording will be
> linked here once available.

[Slides for the talk at CppCon2026](doc/crg_talk_en.pptx)

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
- **Test stages** - nine executable stages (00-08) that build up the framework
  layer by layer, plus the `plugin_loadlibrary` stage demonstrating manual-load
  plugins. Browse them directly in
  [`crg/tests/core_monolith/stages/`](crg/tests/core_monolith/stages/).

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

Each stage is a self-contained `.test.cpp` + `.md` pair that introduces one concept, in
[`crg/tests/core_monolith/stages/`](crg/tests/core_monolith/stages/):

| Stage | `.md` file | Concept |
|---|---|---|
| 00 | [`stage_00_nodelink.md`](crg/tests/core_monolith/stages/stage_00_nodelink.md) | NodeLink auto-registration, Mutual Anonymity |
| 01 | [`stage_01_models.md`](crg/tests/core_monolith/stages/stage_01_models.md) | DenseID, ModelToken, domain |
| 02 | [`stage_02_brain_oop.md`](crg/tests/core_monolith/stages/stage_02_brain_oop.md) | Capability Brain, virtual, `operator->()`, TypeList model-set binding |
| 03 | [`stage_03_routing_1d.md`](crg/tests/core_monolith/stages/stage_03_routing_1d.md) | Contextual 1D axis, EnumTraits, routing is shape-agnostic |
| 04 | [`stage_04_routing_nd.md`](crg/tests/core_monolith/stages/stage_04_routing_nd.md) | N-D routing, branchless Horner |
| 05 | [`stage_05_dynamic_rules.md`](crg/tests/core_monolith/stages/stage_05_dynamic_rules.md) | EarlyExit dispatch cell, RuleContext |
| 06 | [`stage_06_shell_invoke.md`](crg/tests/core_monolith/stages/stage_06_shell_invoke.md) | ModelShell, Invoke/TryInvoke, `Cast<TModel>` |
| 07 | [`stage_07_muscle_dod.md`](crg/tests/core_monolith/stages/stage_07_muscle_dod.md) | Capability Muscle, single-Execute shape -> C-ABI slot |
| 08 | [`stage_08_ffi_trampoline.md`](crg/tests/core_monolith/stages/stage_08_ffi_trampoline.md) | C-ABI proof: a foreign fn ptr fills the Muscle slot |
| plugin_loadlibrary | [`stage_plugin_loadlibrary.md`](crg/tests/plugin_loadlibrary/stage_plugin_loadlibrary.md) | Manual-load plugin: `LoadLibrary` + `CRG_Bootstrap` |

## License

Apache License 2.0 - see `LICENSE` (added at publication time).
