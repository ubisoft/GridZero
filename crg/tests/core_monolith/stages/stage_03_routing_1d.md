# Stage 03 - 1-Dimensional Contextual Routing

## Problem
The same entity type must behave differently depending on a runtime context value (difficulty, game mode, weather, LOD).
A `switch`/`if` branches; a hash map scatters. Both break the branch predictor.

## Solution: add an axis

```cpp
// 1. Axis: enum class + EnumTraits specialization
enum class Difficulty { Easy, Hard };
namespace crg {
    template<> struct EnumTraits<Difficulty> { static constexpr std::size_t Count = 2; };
}

// 2. Routing traits: 1D tensor over Difficulty
namespace crg::routing {
    template<> struct CapabilityRoutingTraits<MyDomain, IMove> {
        using SpaceType = CapabilitySpace<Difficulty>;
    };
}

// 3. Contract: Brain (virtual), same shape as Stage 02 -- routing doesn't care
struct IMove {
    virtual void Move(float& outSpeed) const = 0;
    virtual ~IMove() = default;
};

// 4. Primary Capability = default cell (Easy)
template<typename TModel, typename TAt>
struct DiffMove : public Capability<IMove> {
    void Move(float& outSpeed) const override { outSpeed = 1.0f; }
};

// 5. Partial specialization = Hard cell
template<typename TModel>
struct DiffMove<TModel, At<Difficulty::Hard>> : public Capability<IMove> {
    void Move(float& outSpeed) const override { outSpeed = 3.0f; }
};

namespace {
    static const CapabilityBinding<MyDomain, MyUnit, DiffMove> s_b;
}

// 6. Find() gains a context argument; dispatch is operator->() (Brain)
auto gate = CapabilityRouter<MyDomain>::Find<IMove>(handle, Difficulty::Hard);
float speed = 0.f;
gate->Move(speed);  // speed == 3.0f
```

## Offset computation: Horner's method (branchless)
```
offset = row(DenseID) x Volume + column(enum_value)
```
For 1D: `offset = slot * Count + static_cast<size_t>(value)`. One multiply-add, zero branch.

## Invariants
| Property | Value |
|---|---|
| `Space::Volume` | the axis's `Count` (e.g. 2 for Easy/Hard) |
| `Space::Dimensions` | 1 |
| Branching on the context value | **Zero** |

## Rules for the axis
- A contiguous enum class starting at 0
- `EnumTraits<T>::Count` must match the number of values exactly
- Partial specialization on `At<Val>` overrides the corresponding cell

## Routing is shape-agnostic
`Find()` resolves a cell the same way regardless of whether the contract at that cell is Brain
(virtual, as above) or Muscle.

## When to use
- One context dimension drives behavior (difficulty, quality preset...)
- The full set of values is known at compile time (bounded enum)

## What's new vs Stage 02
`EnumTraits`, `CapabilitySpace<Axis>`, `CapabilityRoutingTraits` specialization, partial specialization of the Capability on `TAt`.
`Find()` gains a context argument.
