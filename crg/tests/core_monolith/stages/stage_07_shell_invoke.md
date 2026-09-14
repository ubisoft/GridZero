# Stage 07 - ModelShell: Brain Interface + Topological Routing

## Problem
A polymorphic OOP contract with several virtual methods + contextual dispatch (the same model behaves differently at Low vs High quality). How do you combine the Brain path with topological routing without losing type safety?

## Solution - `ModelShell<TDomain>` + `Invoke` / `TryInvoke`

`ModelShell<TDomain>` is a fixed-buffer wrapper (64 bytes) that type-erases the concrete type while keeping a DenseID for router lookups. Unlike a classic SBO, there is no heap fallback: a model that's too large fails at compile time (`static_assert`) - it never falls back to a dynamic allocation.

```cpp
S07Unit myUnit{};
ModelShell<S07Domain> shell(myUnit);  // erases the type, keeps the DenseID

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

## The arguments do double duty
The args passed to `Invoke`/`TryInvoke` are used **twice**:
1. **For `Find()`** - computing the offset in the tensor (topological routing)
2. **For the virtual method call** - the method's actual arguments

## Brain contract methods with ModelShell
```cpp
struct IRenderer {
    // Mandatory signature: (const ModelShell<TDomain>&, <context axes>...) const
    virtual std::string_view Describe(const ModelShell<S07Domain>& shell, Quality q) const = 0;
};
```
The shell is available inside the implementation to access model data via `shell.Get<TModel>()`.

## `TryInvoke` for optional bindings
```cpp
// S07Ghost is registered as a type but has no CapabilityBinding
S07Ghost ghost{};
ModelShell<S07Domain> shell(ghost);
auto result = shell.TryInvoke<&IRenderer::Describe>(Quality::Low);
REQUIRE(!result);  // {} - no binding, no crash
```

## Stable type identity
```cpp
const crg::u64 key = shell.GetTypeKey();
// = FNV-1a hash of "S07Unit" - stable across TUs, invariant after copy
```

## When to use
- A type-erased entry point for a system that manipulates heterogeneous entities
- When the caller must not know the concrete type but still needs routing

## What's new vs Stage 05
- `CapabilitySpace<Quality>` adds a 1D tensor over a Quality axis
- `ModelShell` erases the concrete type at the call site
- `Invoke`/`TryInvoke` hide all the routing machinery behind a single templated call
- `TryInvoke` demonstrates the graceful fallback for an unbound type
