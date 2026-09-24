# Stage 06 - ModelShell: Brain Interface + Topological Routing

## Problem
A polymorphic OOP contract with several virtual methods + contextual dispatch (the same model behaves differently at Low vs High quality). The contract is type-erased at the call site — how does the capability implementing it get back to the model's own data at all?

## Solution - the capability calls `shell.Cast<TModel>()`

`ModelShell<TDomain>` is a fixed-buffer wrapper (64 bytes) that type-erases the concrete type while keeping a DenseID for router lookups. Unlike a classic SBO, there is no heap fallback: a model that's too large fails at compile time (`static_assert`) - it never falls back to a dynamic allocation.

The load-bearing call is inside the capability, not at the call site:

```cpp
struct IRenderer {
    // Mandatory signature: (const ModelShell<TDomain>&, <context axes>...) const
    virtual std::string_view Describe(const ModelShell<S06Domain>& shell, Quality q) const = 0;
};

template<typename TModel, typename TAt>
struct UnitRenderer : Capability<IRenderer> {
    std::string_view Describe(const ModelShell<S06Domain>& shell, Quality) const override {
        const TModel& unit = shell.Cast<TModel>();        // <- own TModel, not a hardcoded name
        return (unit.m_Health > 0.f) ? "alive" : "dead";  // <- real model data, not a fixed string
    }
};
```

`Cast<TModel>()` uses the capability's **own** template parameter, not a type name baked in at
authoring time — that is what keeps the same capability body reusable if it is ever bound across
a `TypeList` of models (Stage 02): each instantiation casts back to its own `TModel`.

Two `ModelShell`s wrapping two `S06Unit`s with different `m_Health` produce different output at
the *same* quality tier — proof the cast is real, not a decorative parameter the implementation
ignores.

## Calling it: `Invoke` / `TryInvoke`
This is secondary to the `Cast<TModel>()` call above, but it is how a caller reaches the
capability:

```cpp
S06Unit myUnit{};
ModelShell<S06Domain> shell(myUnit);  // erases the type, keeps the DenseID

// Invoke<&IFoo::Method>(args...):
// - args are forwarded to Find() to compute the topological offset
// - args are forwarded to the virtual method as call arguments
// - asserts if no binding exists
auto low  = shell.Invoke<&IRenderer::Describe>(Quality::Low);
auto high = shell.Invoke<&IRenderer::Describe>(Quality::High);

// TryInvoke<&IFoo::Method>(args...):
// - returns std::optional<R>
// - returns {} if the shell is empty or no binding is registered
auto result = shell.TryInvoke<&IRenderer::Describe>(Quality::High);
if (result.has_value()) { /* ... */ }
```

The args passed to `Invoke`/`TryInvoke` do double duty: forwarded to `Find()` to compute the
tensor offset (topological routing), then forwarded again to the virtual method as its actual
call arguments.

## `TryInvoke` for optional bindings
```cpp
// S06Ghost is registered as a type but has no CapabilityBinding
S06Ghost ghost{};
ModelShell<S06Domain> shell(ghost);
auto result = shell.TryInvoke<&IRenderer::Describe>(Quality::Low);
REQUIRE(!result);  // {} - no binding, no crash
```

## Stable type identity
```cpp
const crg::u64 key = shell.GetTypeKey();
// = FNV-1a hash of "S06Unit" - stable across TUs, invariant after copy
```

## When to use
- A type-erased entry point for a system that manipulates heterogeneous entities
- When the caller must not know the concrete type, but the capability still needs the model's
  real data, not just its routed coordinate

## What's new vs Stage 05
- `CapabilitySpace<Quality>` adds a 1D tensor over a Quality axis
- `ModelShell` erases the concrete type at the call site, and the capability undoes that erasure
  with `shell.Cast<TModel>()` to read real model data
- `Invoke`/`TryInvoke` hide the routing machinery behind a single templated call
- `TryInvoke` demonstrates the graceful fallback for an unbound type
