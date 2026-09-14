# config.hpp — Design Notes

## OS-agnostic contract

`config.hpp` is deliberately free of any platform-detection macros (`#ifdef _WIN32`, etc.).
All values that differ per OS or per build mode are **injected by the build system** (Sharpmake/CMake) as compiler defines:

- `CRG_SHARED_LIB_PREFIX` / `CRG_SHARED_LIB_EXTENSION` — shared library naming convention (e.g. `"lib"` / `".so"` on Linux, `""` / `".dll"` on Windows). Missing either is a hard build error.
- `CRG_ASSERT_ENABLED` — `1` in Debug, `0` in Profile/Release. Defaults to `1` if omitted (safe fallback).
- `CRG_PLUGINS_ENABLED` — enables DLL/plugin loading; defaults to `0` (static build).

This pattern keeps the core headers portable and makes platform differences an explicit build-system concern rather than a preprocessor maze inside source files.
