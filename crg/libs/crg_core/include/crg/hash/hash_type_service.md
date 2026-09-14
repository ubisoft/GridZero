# IHashTypeService — Design Rationale

`IHashTypeService` is a Brain Capability (virtual interface, off hot path) that exposes type-name hashing over a stable C++ vtable ABI.

## Why it exists

Native C++ plugins can call `HashString` directly (the constexpr FNV-1a in `crg/core/hash.hpp`) with zero overhead. Guest runtimes — Rust crates and WASM modules — cannot link against `constexpr` host code; they need a stable ABI boundary. This interface provides that boundary: the host implements it once and exposes it through the capability system; guests resolve it via `CRG_DECLARE_CONTRACT` and call through the vtable.

## What it is not

Not intended for use by any native C++ plugin. If you are writing C++ code and find yourself binding this capability, prefer `HashString` directly.
