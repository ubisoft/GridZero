# Stage 04 - N-Dimensional Contextual Routing (Horner's Method)

## Problem
Behavior depends on **several independent axes simultaneously** (Battery x Terrain x Mode).
A nested `if/switch` tree grows as O(N1xN2xN3). A hash map collapses the dimensions but adds a per-call hashing cost.

## Solution: `CapabilitySpace<A, B, C...>`

```cpp
// Three independent axes
enum class Battery { Critical, Low, Nominal };   // Count = 3
enum class Terrain { Flat, Rough };               // Count = 2
enum class Mode    { Eco, Performance };          // Count = 2

// Routing traits: 3D tensor - 3 x 2 x 2 = 12 cells
namespace crg::routing {
    template<> struct CapabilityRoutingTraits<MyDomain, ILocomotion> {
        using SpaceType = CapabilitySpace<Battery, Terrain, Mode>;
    };
}

// Contract: Brain (virtual), same shape as Stages 02-03
struct ILocomotion {
    virtual void Drive(float& outSpeed, bool& outActive) const = 0;
    virtual ~ILocomotion() = default;
};

// Primary template = default behavior for all 12 cells
template<typename TModel, typename TAt>
struct DriveCapability : public Capability<ILocomotion> {
    void Drive(float& outSpeed, bool& outActive) const override { outSpeed = 1.0f; outActive = true; }
};

// Override a single corner: Nominal x Flat x Performance
template<typename TModel>
struct DriveCapability<TModel, At<Battery::Nominal, Terrain::Flat, Mode::Performance>>
    : public Capability<ILocomotion> {
    void Drive(float& outSpeed, bool& outActive) const override { outSpeed = 10.0f; outActive = true; }
};

// Find() gains N context arguments; dispatch is operator->() (Brain)
auto gate = CapabilityRouter<MyDomain>::Find<ILocomotion>(
                handle, Battery::Nominal, Terrain::Flat, Mode::Performance);
float speed = 0.f; bool active = false;
gate->Drive(speed, active);
```

## Offset computation: Horner's method (branchless)
```
offset = (...((v0 x |A1|) + v1) x |A2| + v2 ...) x |AN-1| + vN-1
```
- A single multiply-add pass
- Zero branching, zero hashing
- Fully computed at compile time for constexpr values

Worked example: `At<Battery::Nominal, Terrain::Flat, Mode::Performance>` is indices `(2, 0, 1)`
against counts `(3, 2, 2)`. Applying Horner left-to-right: `((2 * 2) + 0) * 2 + 1 = 9` — tensor
index 9.

## Invariants
| Property | Value |
|---|---|
| `Space::Volume` | Product of all `Count`s (e.g. 3x2x2 = 12) |
| `Space::Dimensions` | Number of axes |
| API identical to Stage 03 | **Yes** - Find() just gains one extra argument per axis |

## `At<v0, v1, v2>` - multi-dimensional coordinate
The partial specialization takes `At<val0, val1, val2>` as TAt.
Semantics are identical to Stage 03 - only the number of values changes.

## Watch out: combinatorial explosion
Volume = product of all Counts. With 4 axes of Count=4: **256 cells**.
Use this only when the total volume stays reasonable.

## The full matrix builds itself: models x axis-cells
Stage 02 showed that `CapabilityBinding`'s `TModel` slot can be a single model or a
`TypeList<...>` of models, fanning one capability declaration across all of them. Combine that
with an N-D axis here and one declaration produces the complete matrix — every listed model x
every axis cell — with no additional code per model.

## Data layout: this already has ECS shape
The routing tensor is a flat array indexed by `DenseID * Volume + offset` — a Structure-of-Arrays
keyed by a dense integer id, the same shape an ECS component store uses. CRG's dispatch mirrors
an ECS's data layout rather than competing with it.

## What's new vs Stage 03
Multiple axes in `CapabilitySpace`. `Find()` takes N context arguments.
`TAt` becomes `At<v0, v1, v2>`.
