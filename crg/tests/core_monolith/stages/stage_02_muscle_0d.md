# Stage 02 - Muscle Capability: DOD Path, 0-Dimensional Routing

## Problem
Bind a stateless function to a model type and invoke it via its token - **zero heap allocation, zero virtual call on the hot path**.

## Solution - Muscle Path (Data-Oriented Design)

```
Contract    = struct with a nested Params struct (no virtual)
Capability  = template<TModel, TAt> with static Execute(Params&)
Binding     = static CapabilityBinding in an anonymous namespace
Find        = returns CapabilityHandle<TContract> -> operator()
```

```cpp
// Contract: flat struct with Params - IsStaticContract<IMove> == true
struct IMove {
    struct Params { float m_Speed{ 0.f }; };
};

// Capability: CRTP template, static Execute is mandatory
template<typename TModel, typename TAt>
struct DefaultMove : public Capability<IMove> {
    static void Execute(IMove::Params& p) { p.m_Speed = 1.0f; }
};

// Binding: a single line, anonymous namespace, zero Init()
namespace {
    static const CapabilityBinding<MyDomain, MyUnit, DefaultMove> s_b;
}

// Dispatch: ~1 ns, one array access + one indirect call
auto token = ModelToken<MyDomain>::FromType<MyUnit>();
auto gate  = CapabilityRouter<MyDomain>::Find<IMove>(token);
IMove::Params p;
gate(p);  // operator() -> Execute
```

## `CapabilitySpace<>` - 0D tensor
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
- Stateless per-type logic with no contextual variation
- Hot-path budget: ~1 ns per call

## What's new vs Stage 01
Contract + Capability template + `CapabilityBinding` + `CapabilityRouter`.
The routing tensor exists but has exactly **one cell per registered model**.

## Custom return values

`Execute` defaults to `void`. A contract opts in to a non-`void` return by
declaring a nested `Result` type; `StaticResultOf<TContract>` picks it up and
`operator()`/`TryInvoke` return that type instead:

```cpp
struct IHealthQuery {
    struct Params { std::uint32_t m_Id{ 0 }; };
    using Result = int;
};

template<typename TModel, typename TAt>
struct DefaultHealthQuery : public Capability<IHealthQuery> {
    static int Execute(IHealthQuery::Params& p) { return static_cast<int>(p.m_Id) * 2; }
};

int value = gate(p);                       // operator() returns Result directly
std::optional<int> maybe = gate.TryInvoke(p); // TryInvoke wraps Result in std::optional
```

`TryInvoke` wraps the same way `ModelShell::TryInvoke` does
(`crg::TryInvokeResult_t<Result>`): `void` stays `void`, a non-optional
`Result` becomes `std::optional<Result>`, and a `Result` that is already
`std::optional<T>` is returned as-is - never double-wrapped.
