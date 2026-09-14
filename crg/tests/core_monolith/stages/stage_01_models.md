# Stage 01 - Models : Typed Entity Handles (DenseID + ModelToken)

## Problem
Dispatching to the right function for a given entity type requires knowing that type at runtime.
A vtable works but chains through a heap pointer on every call and scatters data across memory.

## Solution: `DenseID` + `ModelToken<TDomain>`

Every model type declared via `CRG_DECLARE_DOMAIN_MODELS` injects a `ModelNode<Domain, TypeHash<T>::Value>` node into the domain's `NodeLink` chain at static-init. `DenseIndexStore<Domain>` is a **read-only hash -> slot directory** that `DomainSynchronizer` populates append-only via `Refresh()` on plugin load/unload. The slot is simply the hash's position in the vector - a small contiguous integer (0, 1, 2...), stable for life.

`ModelToken<TDomain>` wraps this index. The routing tensor uses it as a row offset: **a single flat array access, zero pointer chasing**. `ModelToken::FromType<T>()` is a pure `Find` - lock-free, never an allocation.

```cpp
// Domain - isolated routing universe (empty tag struct)
struct MyDomain {};
CRG_DECLARE_DOMAIN(MyDomain)
CRG_DEFINE_DOMAIN(MyDomain)   // empty in monolithic mode, active in DLL mode

// Entity types + one-line declaration
struct Robot { crg::u8 m_Data[8]; };
struct Drone { crg::u8 m_Data[8]; };
CRG_DECLARE_DOMAIN_MODELS(MyDomain, Robot, Drone)

// Usage
auto robot = ModelToken<MyDomain>::FromType<Robot>();  // stable DenseID
auto drone = ModelToken<MyDomain>::FromType<Drone>();

assert(robot.GetDenseIndex() != drone.GetDenseIndex());  // distinct IDs
assert(robot.GetDenseIndex() < 2);                        // small contiguous integers
```

## Domain: a total-isolation universe
- Empty tag struct - every capability and trait declared under this domain is **fully isolated** from other domains.
- Two domains can reuse the same model or contract types without conflict.

## Invariants
| Property | Value |
|---|---|
| `FromType<T>()` idempotent | **Yes** - pure `Find`, two calls yield the same slot |
| DenseID values | Small non-negative contiguous integers (0, 1, ...) |
| `CRG_DECLARE_DOMAIN_MODELS` required | **Yes** - without it `CapabilityBinding<D, T, ...>` won't compile (`static_assert sizeof(ModelKey<T>) > 0`) |
| Slot of an already-published model | **Never reassigned** - `Refresh` is append-only |
| `Find` thread safety | **Lock-free** - only `Refresh` mutates, gated by `DomainSynchronizer` |
| Unknown hash | `Find` returns `InvalidDenseSlot`, store unchanged - no silent allocation |

## Domain and model identity - the same convention

| | Hash |
|---|---|
| Domain (`CRG_DECLARE_DOMAIN`) | full text `#TDomain`, untruncated |
| Model (`CRG_DECLARE_MODEL`) | full text `#ModelType`, untruncated |

Both hash the full qualified name - the same language-neutral isolation axis (Rust/WASM guests only exchange `u64` via `crg_router_find`). Two distinct types sharing the same leaf within the same domain (`studio::Scout` vs `mod::Scout`) therefore hash to different slots; the collision that remains possible is the FNV-1a 64-bit collision on the full name (astronomically improbable under co-compilation), not a truncated-leaf collision.

## When to use (without going further)
- A stable, compact per-type identity (e.g. a key for type-indexed maps)
- Building the routing infrastructure without dispatching yet

## What's new vs Stage 00
`CRG_DECLARE_DOMAIN`, `CRG_DECLARE_MODEL`, `ModelToken`. No capability or routing tensor yet.
