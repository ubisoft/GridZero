# Contributing to GridZero

Thanks for taking a look at GridZero. This document covers how to build the
project, what a change needs before it can be accepted, and a few conventions
the codebase relies on.

## Building and testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/crg/tests/core_monolith/Run_CoreMonolith_Tests
./build/crg/tests/plugin_loadlibrary/Run_PluginLoadLibrary_Tests
```

C++17, no external dependencies for `crg_core`. Run the test binaries before
opening a PR — a change that regresses an existing stage will be asked to fix
the stage, not skip it.

Internally, GridZero's project files are also generated with
[Sharpmake](https://github.com/ubisoft/Sharpmake) — Ubisoft's own open-source,
Apache-2.0-licensed project generator. That path isn't part of this
repository; the CMake build above is the complete, standalone way to build
`crg_core` and its tests.

## One file = one doc + tests

Every header under `crg/` is expected to ship with:

- a companion `<header-name>.md` next to it, explaining the *why* — the
  invariant it protects, the trade-off it makes, the gotcha a reader would
  otherwise hit. Not a restatement of what the code already says.
- dedicated unit tests exercising it, in `crg/tests/`. A new capability,
  trait, or routing primitive without a test attached will not be merged.

This applies per file, not per directory — a PR that touches one header only
needs that header's doc/tests in order, not a full-directory audit.

## Conventions

- **English only** — comments, docs, and identifiers. No exceptions.
- **No OS-specific code outside a platform TU.** `crg/core` and `crg/libs`
  stay OS-agnostic: no `#ifdef _WIN32`, no `Win64`/`Linux`/`Posix`/`x86` in
  identifiers. Platform-specific code belongs in its own
  `src/platform_<os>/` translation unit.
- **No `k`-prefix on `constexpr`**, and member variables use `m_PascalCase`.
- Formatting follows `.clang-format` at the repo root — run it before
  submitting.
- **Zero allocation on the hot path.** No `new`, no `std::make_shared`/
  `std::shared_ptr`, no `std::string` by value, no `std::map`/
  `std::unordered_map` — use arenas, `std::string_view`, and flat arrays
  indexed by `DenseID`.
- **No virtual dispatch on the Muscle hot path** — route through a
  `CapabilityHandle`/function pointer instead. Virtual calls are fine off
  the hot path (Capability Brain).
- **No `GodRegistry`, `Manager`, or `Singleton`, and no explicit `Init()`/
  `Register()`.** Wiring is auto-registration via `NodeLink` (a
  `static const` instance at namespace scope) — see stage 00.
- **Maximum one level of concrete inheritance** above an interface. No
  factory, no builder; model behavioral variation with Capabilities, not a
  class hierarchy.
- **Branchless on the hot path** where a branch would otherwise depend on
  routing state — prefer arithmetic/bitmask forms over `if`.
- **`constexpr` everywhere possible** — data hashes, tensor sizes, routing
  coordinates.

These are enforced in review, not just a style preference.

## License header

New source files need the two-line notice at the top (see any existing
`.hpp`/`.cpp` for the exact wording):

```cpp
// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.
```

## Keep it self-contained

Don't include local absolute paths, machine hostnames, or references to
tooling/systems outside this repository — a contribution should build and
make sense from this tree alone.

## Pull requests

Open a PR against `main` with a clear description of the behavior change and
which stage/test covers it. This repository mirrors from an internal
development pipeline, so review can take a little longer than a typical
GitHub project, and a maintainer may occasionally land your change by hand
(with attribution) rather than merging directly — that's normal here, not a
sign something went wrong.
