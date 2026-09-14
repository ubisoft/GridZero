# dense_index — design notes

## Purpose

`DenseIndexStore<TDomain>` is a read-only directory of model hashes for a domain. It maps a
`u64` hash to a stable integer slot (`u32`). Once assigned, a slot never changes — subsequent
`Refresh()` calls are append-only.

## Threading contract

- `Refresh()` — single writer, single thread. Called by `DomainSynchronizer` before populators
  run (bootstrap, plugin load, plugin unload). Walks the `ModelNodeBase<TDomain>` chain.
- `Find(hash)` / `Size()` — read-only, lock-free. Safe to call from any thread after bootstrap.

## Bootstrap idiom

Mirrors `NodeLink::Linearizer`: a private `Bootstrap` struct whose constructor calls `Refresh()`
once. Exposed through a function-local `static const` accessor (`GetBootstrap()`). Every public
entry point touches the instance via `[[maybe_unused]] const Bootstrap& trigger = GetBootstrap();`
to guarantee one-shot initialization on first access.

## No Intern / no GetOrCreate

There is intentionally no runtime insertion path. Runtime code (POX, JSON loader, WASM contracts)
calls `ModelToken::FromHash(h)`, which is a pure `Find`. An unknown hash returns
`InvalidDenseSlot`; `ModelToken::IsValid()` is false; the router safely short-circuits.

C++ bindings declared with `CRG_DECLARE_DOMAIN_MODELS` have their hash baked at compile time, so
by the time `Populate` runs the slot is guaranteed present (asserted on the spot).

## TypeHash stability

The XOR constant `0x5a3c75939a3779b9` in the `TypeHash` specialization matches the legacy
`DenseIDStore<TDomain>` key, keeping existing `UniversalAnchor` ABI keys stable across the rename.
