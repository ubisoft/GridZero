# dense_id — design notes

`DenseModelID<TDomain>` wraps a `u32` dense slot index. Slots are produced by
position in the `ModelNodeBase<TDomain>` NodeLink chain, fed at
`DomainSynchronizer` tick and never mutated outside of it.

## No GetOrCreate

A `DenseModelID` has exactly two states:

- **Valid** — the slot returned by `DenseIndexStore<TDomain>::Find(hash)` for a
  hash declared via `CRG_DECLARE_DOMAIN_MODELS`.
- **Invalid** (`InvalidModelSlot = u32::max`) — the hash has not been declared.

There is no creation path at lookup time. The mapping directory lives in
`DenseIndexStore<TDomain>` (`crg/core/dense_index.hpp`).

## Include-cycle note

`InvalidModelSlot` is defined here rather than in `dense_index.hpp` to avoid a
circular dependency: `dense_index.hpp` includes `ModelNodeBase`, and
`ModelNodeBase` must not depend on `dense_index.hpp`.
