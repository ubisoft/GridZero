# ModelToken — Design Notes

`ModelToken<TDomain>` is the runtime-resolved model identity consumed by `CapabilityRouter`.
It wraps a single dense slot index — no ownership, no lifetime, no indirection. Internal
storage (`DenseID`) is private; `GetDenseIndex()` is a public accessor for callers that
need the raw slot (routing, bridge code to a foreign u64 identifier). Callers that only
need identity comparison should prefer `operator==`/`operator!=`.

## Production

Tokens are produced exclusively by a pure read-only `Find` against the per-domain
`DenseIndexStore` — never by allocation. Factory methods:

- `FromType<TModel>()` — compile-time path: `TypeHash<TModel>` is baked at instantiation.
  `DomainTraits::TypeKey` `static_assert`s that the model was registered via
  `CRG_DECLARE_DOMAIN_MODELS`. The slot is resolved at runtime against the populated store;
  the assertion covers the *declaration*, not the *bootstrap timing*.
- `FromKey(u64)` — runtime path: pure `Find`. An unknown hash returns an invalid token;
  the store is never mutated.
- `FromHash(u64)` — semantic alias for loader paths that derive a key from a sidecar value
  or from `TypeHash<DefaultPluginModel>`.
- `Set(u64)` — mutator form of `FromKey`, for callers that default-construct a token first
  and resolve it later.

Direct slot construction (bypassing the store lookup) is a private constructor, friend-only
to `CapabilityRouter` — its own `ValidateContract()` is the sole legitimate caller, re-deriving
a token for every already-resolved slot in the store. Not exposed as a public factory: no
test or benchmark gets to fabricate a token from an arbitrary slot.

## Validity

An unknown hash yields `IsValid() == false`. `CapabilityRouter` safely short-circuits on
invalid tokens, allowing loaders to fall back on their own diagnostics.
