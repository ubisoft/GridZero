# Stage 00 - Linker-Driven Discovery (NodeLink)

## Problem
Several implementations of an interface are scattered across separate `.cpp` files.
The caller must visit all of them **without knowing how many there are or where they live**.
No central registry, no `Init()` call, no header coupling.

## Solution: `NodeLink<TNode, TContract>`

```cpp
// 1. The interface
struct IGreeter { virtual const char* Greet() const = 0; };

// 2. The list type (implicit registry wired by the linker)
struct IGreeterList : crg::discovery::NodeLink<IGreeterList, IGreeter> {};

// 3. Implementations in any .cpp - a static variable = auto-registration
namespace {
    struct HelloGreeter : IGreeterList {
        const char* Greet() const override { return "Hello"; }
    };
    static const HelloGreeter s_Hello;  // <- chains in at startup, no Init()
}

// 4. The caller iterates without knowing anything about the implementations
IGreeterList::Visit([](IGreeterList& node) { node.Greet(); });
```

## Mechanism
- Each `static const MyNode s_foo` invokes `NodeLink`'s constructor, which **inserts itself into an intrusive linked list** at program startup (before `main`).
- `Visit()` iterates the linearized cache of that chain. If a DLL is loaded, `RefreshCache()` walks the chain again to pick up the new nodes.
- **Mutual Anonymity**: the caller and the implementers share no internal header. Wiring is resolved by link topology.

## Invariants
| Property | Value |
|---|---|
| Allocation on the hot path | **Zero** |
| Vtable on the hot path | One (the interface) |
| Header coupling caller<->implementer | **None** |
| Explicit registration required | **No** |

## Cancellable `Visit`
```cpp
IGreeterList::Visit([](IGreeterList& node) -> bool {
    // returning false stops the iteration (find-first)
    return shouldContinue;
});
```

## When to use (without going further)
- A service locator without a singleton
- Collecting every subsystem registered at startup
- A plugin system where the caller must not know the implementers

## What's new vs the previous stage
Everything. This is Layer 2 (Discovery) in isolation. No domain, no model, no routing tensor.
