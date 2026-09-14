# abi_types — ABI boundary design rationale

## Why STL types are forbidden at the host/plugin boundary

Anything that crosses a JIT-compiled `.dll`/`.so` boundary at runtime must be
trivially-copyable and standard-layout. STL types (`std::string`, `std::vector`)
carry implementation-defined internals — SBO thresholds, allocator state,
debug-iterator slots — that differ between MSVC, libstdc++, and libc++. Passing
them through the boundary risks memory corruption when host and plugin disagree on
their layout.

## Pattern: raw pointer + explicit length

Each transport type pairs a raw pointer with an explicit `uint64_t` length/count
and provides ergonomic conversions to/from `std::string_view` / `std::span`, so
callers retain a modern C++ DX in-process while staying ABI-clean across the
boundary.

## Types

- **StringView** — non-owning `const char*` + `uint64_t` length; converts to/from `std::string_view`.
- **Span\<T\>** — non-owning `const T*` + `uint64_t` count; range-for friendly.
- **Logger** — C-ABI callback (`LogCallbackFunc`) + opaque `void* userData`; the callee owns no buffer and the caller decides the sink (stderr, UI panel, discard).
