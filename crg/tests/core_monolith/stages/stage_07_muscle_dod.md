# Stage 07 - Muscle Capability: DOD Path, unrouted

## Problem
You want a stateless function bound to a model type and invoked via its token, with an
interface that is strictly a single entry point — not several named methods.

## Solution - Muscle Path (Data-Oriented Design)

```
Contract    = struct with a nested Params struct (no virtual)
Capability  = template<TModel> with static Execute(Params&)
Binding     = static CapabilityBinding in an anonymous namespace
Find        = returns CapabilityHandle<TContract> -> operator()
```

```cpp
// Contract: flat struct with Params - IsStaticContract<IMove> == true
struct IMove {
    struct Params { float m_Speed{ 0.f }; };
};

// Capability: unrouted -> arity-1, no TAt. static Execute is mandatory.
template<typename TModel>
struct DefaultMove : public Capability<IMove> {
    static void Execute(IMove::Params& p) { p.m_Speed = 1.0f; }
};

// Binding: a single line, anonymous namespace, zero Init()
namespace {
    static const CapabilityBinding<MyDomain, MyUnit, DefaultMove> s_b;
}

// Dispatch: one array access + one indirect call through operator()
auto token = ModelToken<MyDomain>::FromType<MyUnit>();
auto gate  = CapabilityRouter<MyDomain>::Find<IMove>(token);
IMove::Params p;
gate(p);  // operator() -> Execute
```

## Why Muscle, if not for speed
`dod_vs_oop.bench.cpp` measures this against the Brain/virtual path and finds a **tie**
(6.80 vs 6.65 ns) — mechanism-alone tax is effectively zero. The reason to reach for Muscle is
the *shape* of the contract: `HasStaticExecute` enforces a single `Execute(Params&)` entry point,
never several named methods (that is the Brain path). A single entry point collapses to one
plain function pointer — a C-ABI slot, not a C++ vtable slot.

## `CapabilitySpace<>` - unrouted (0D) tensor
No axis -> Volume = 1. `Find()` takes no context argument.
The function pointer is stored directly in the `CapabilityHandle`.

## Hot path cost
| Operation | Cost |
|---|---|
| `Find()` | 1 array access (DenseID x Volume) |
| `gate(p)` | 1 indirect call (function pointer) |
| Allocation | **Zero** |
| Virtual call | **Zero** |

## DOD Contract rules
- Must have a nested `struct Params`
- Must **not** be polymorphic (`std::is_polymorphic_v == false`)
- -> `IsStaticContract<T> == true` -> `CapabilityHandle` exposes `operator()`

## When to use
- The interface is naturally a single entry point, not several methods
- The binding needs to reduce to a plain C-ABI function pointer

## What's new vs Stage 06
Contract + Capability template with a strictly single-`Execute` shape (vs Brain's multi-method
`operator->()`, Stage 02). Unrouted arity-1, no `TAt`.

## Custom return values

`Execute` defaults to `void`. A contract opts in to a non-`void` return by declaring a nested
`Result` type; `StaticResultOf<TContract>` picks it up and `operator()`/`TryInvoke` return that
type instead — `TryInvoke` wraps it the same way `ModelShell::TryInvoke` does
(`crg::TryInvokeResult_t<Result>`): `void` stays `void`, a non-optional `Result` becomes
`std::optional<Result>`, and a `Result` that is already `std::optional<T>` is returned as-is,
never double-wrapped. See `routing/contract_result_traits.test.cpp` for full coverage.
