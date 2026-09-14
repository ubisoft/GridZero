# model_key — Design Notes

## Macro hierarchy

Three macros form a layered registration system:

| Macro | What it emits |
|---|---|
| `CRG_DECLARE_MODEL(M)` | `TypeHash<M>` (FNV-1a of type name) + `ModelKey<M>` specialization. Use alone only when `ModelDomain<M>` is not needed (e.g. hash-only testing). |
| `CRG_DECLARE_DOMAIN_MODELS(D, M...)` | Calls `CRG_DECLARE_MODEL` per model **plus** `ModelDomain<M>{ using Type = D; }`. The `ModelDomain` bridge is required by `PipelineCapability<TModel, At<Phase>>` to recover the owning domain without threading it through every call-site. The domain `D` must have been declared via `CRG_DECLARE_CAPABILITY_DOMAIN` or `CRG_DECLARE_PIPELINE_DOMAIN` first. Capacity: 16 models per call — split across multiple invocations if more are needed. |
| `CRG_DECLARE_FORMAT(FmtName, ExtStr)` | Generates `crg::formats::FmtName` as a zero-size tag type whose routing key is `HashStringLower(ExtStr)`. Lets the routing layer dispatch by file-extension hash without any `if (ext == ".json")` in the core. |

## ModelNode and linker-driven DenseID assignment

`CRG_DECLARE_DOMAIN_MODELS` also emits an `inline const ModelNode<D, Hash>` per model (via `CRG_DDM_BIND`). Each TU that re-includes the same declaration contributes one node to the intrusive linked list at static-init time. `DenseIndexStore` deduplicates by hash so exactly one dense slot is assigned per model regardless of how many TUs include the header.

The node variable is named with `__COUNTER__` because the model type expression may contain `::`, which is illegal in C++ identifiers.

## `ModelKey<TModel>` incomplete-by-default invariant

`ModelKey` is declared but not defined for the primary template, forcing a compile error for any unregistered type. Specializations are only created by the macros above, ensuring every type that participates in capability routing has a stable hash.
