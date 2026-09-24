# Stage 02 - Brain Capability: OOP / Polymorphic Path

## Problem
Some contracts are naturally polymorphic: several named virtual methods, object semantics.
Forcing a `Log()` + `Serialize()` + `Debug()` interface into a single `Execute(Params&)` is clumsy and loses clarity.

## Solution - Brain Path

```cpp
// Brain Contract: virtual methods, no Params
// std::is_polymorphic_v<ILogger> == true
// -> IsStaticContract<ILogger>       == false
// -> CapabilityHandle stores const ILogger* and exposes operator->()
struct ILogger {
    virtual void Log(const char* msg) const = 0;
    virtual int  Level()             const = 0;
    virtual ~ILogger() = default;
};

// Brain Capability: inherits the virtual interface directly
// Arity-1 -- no TAt. Stage 02 has no axis; TAt is introduced at Stage 03.
template<typename TModel>
struct ConsoleLogger : Capability<ILogger> {
    void Log(const char* msg) const override { (void)msg; }
    int  Level()             const override { return 1; }
};

namespace {
    static const CapabilityBinding<MyDomain, MyUnit, ConsoleLogger> s_b;
}

// Dispatch: operator->() instead of operator()
auto gate = CapabilityRouter<MyDomain>::Find<ILogger>(handle);
gate->Log("message");   // one pointer dereference + one virtual call
int lvl = gate->Level();
```

## Model-set binding: one model, or a `TypeList` of models

`CapabilityBinding<TDomain, TModel, TCapabilities...>`'s `TModel` slot accepts either a single
model or a `crg::TypeList<TModels...>`. The `TypeList` form fans the *same* capability set across
*every* listed model from one declaration — the matrix (models x capabilities) builds itself,
no per-model repetition:

```cpp
using Squad = crg::TypeList<Scout, Drone>;
namespace { static const CapabilityBinding<MyDomain, Squad, ConsoleLogger> s_squad; }
// One declaration binds ConsoleLogger for BOTH Scout and Drone.
// Symmetric with the single-model form above -- same TModel slot, same syntax.
```

See `capability_binding.md` ("`TModel = TypeList<TModels...>`") for the mechanism: this is a
partial specialization that derives from `CapabilityBinding<TDomain, TModels, TCaps...>...`, one
base per model, each independently self-registering into the arena.

## Muscle vs Brain - a compile-time decision, zero cost
```cpp
// Muscle: struct Params, not polymorphic -> IsStaticContract == true -> operator()
struct IMove { struct Params { float m_Speed; }; };
static_assert(IsStaticContract<IMove> == true);

// Brain: polymorphic -> IsStaticContract == false -> operator->()
struct ILogger { virtual void Log(const char*) const = 0; };
static_assert(IsStaticContract<ILogger> == false);
```
The choice is a compile-time SFINAE. Zero cost for the path not taken.

## Brain hot path cost
| Operation | Cost |
|---|---|
| `Find()` | 1 array access (same as Muscle) |
| `gate->Method()` | 1 pointer dereference + 1 virtual call |
| Heap allocation | **Zero** (pointer to the static instance) |

## When to use Brain vs Muscle
| Brain | Muscle |
|---|---|
| Contract with several named methods | Contract with a single `Execute(Params&)` |
| Natural OOP semantics (render, serialize, debug) | Extreme hot path (< 1ns) |
| Not on the nanosecond hot path | Stateless logic, a single entry point |

## What's new vs Stage 01
First capability contract and binding. Virtual contract -> `IsStaticContract == false` -> `CapabilityHandle` gains `operator->()` instead of `operator()`.
