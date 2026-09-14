# domain_traits.hpp — Design Notes

## Purpose

`DomainTraits<TDomain>` is the central compile-time contract that every Routing Domain must satisfy. It exposes:

- **`Identity`** — a `u64` key uniquely identifying the domain, sourced from `DomainKey<TDomain>::Value`.
- **`TypeKey<TModel>`** — a `u64` key resolving a model type within this domain, sourced from `ModelKey<TModel>::Value`.

## Lazy (ODR-deferred) validation

Both `Identity` and `TypeKey<T>` are checked lazily: the `static_assert` inside `ComputeIdentity()` and `ResolveModelKey<T>()` fires only when those members are odr-used. This is intentional: a domain tag type may be passed as a `ModelShell` template parameter without triggering `CRG_DECLARE_DOMAIN()` registration, as long as `Identity` is never accessed.

## Fixed-size buffer sizing

`ModelShell`'s fixed-size buffer sizing (no heap fallback — oversized payloads fail to compile) is **not** part of `DomainTraits`. It lives in `crg::shell::ModelShellTraits<TDomain>`.

## Registration macros

- `CRG_DECLARE_DOMAIN()` — registers a domain and populates `DomainKey<TDomain>`.
- `CRG_DECLARE_DOMAIN_MODELS(TDomain, M1, M2, ...)` — registers models and populates `ModelKey<Mn>`.
