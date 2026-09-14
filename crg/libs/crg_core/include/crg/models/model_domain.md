# model_domain — Design Notes

## Purpose

`ModelDomain<TModel>` is a trait that maps a Model type to the Domain that owns it.
`ModelDomainT<TModel>` is the convenience alias for `ModelDomain<TModel>::Type`.

## How it is populated

`CRG_DECLARE_DOMAIN_MODELS(TDomain, M1, M2, ...)` specializes `ModelDomain` for each listed model, setting `Type = TDomain`.

## Why it exists

`PipelineCapability<TModel, At<Phase>>` needs to reach `TaskGraph<Domain>` without requiring every call-site to thread the domain type explicitly. `ModelDomainT<TModel>` provides that recovery from the model alone.

## Intentional incompleteness

The primary template has no definition. Any attempt to instantiate `ModelDomain<X>` for a model that was never passed to `CRG_DECLARE_DOMAIN_MODELS` results in a **template-not-defined** compile error, which names the offending type and guides the developer to register the model.
