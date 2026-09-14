# Stage 05 - Brain Capability: OOP / Polymorphic Path

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
template<typename TModel, typename TAt>
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

## What's new vs Stages 02-04
Virtual contract -> `IsStaticContract == false` -> `CapabilityHandle` gains `operator->()` instead of `operator()`.
Everything else stays identical.
