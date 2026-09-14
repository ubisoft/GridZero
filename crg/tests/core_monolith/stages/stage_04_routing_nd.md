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

// Primary template = default behavior for all 12 cells
template<typename TModel, typename TAt>
struct DriveCapability : public Capability<ILocomotion> {
    static void Execute(ILocomotion::Params& p) { p.m_Speed = 1.0f; }
};

// Override a single corner: Nominal x Flat x Performance
template<typename TModel>
struct DriveCapability<TModel, At<Battery::Nominal, Terrain::Flat, Mode::Performance>>
    : public Capability<ILocomotion> {
    static void Execute(ILocomotion::Params& p) { p.m_Speed = 10.0f; }
};

// Find() gains N context arguments
auto gate = CapabilityRouter<MyDomain>::Find<ILocomotion>(
                handle, Battery::Nominal, Terrain::Flat, Mode::Performance);
```

## Offset computation: Horner's method (branchless)
```
offset = (...((v0 x |A1|) + v1) x |A2| + v2 ...) x |AN-1| + vN-1
```
- A single multiply-add pass
- Zero branching, zero hashing
- Fully computed at compile time for constexpr values

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

## What's new vs Stage 03
Multiple axes in `CapabilitySpace`. `Find()` takes N context arguments.
`TAt` becomes `At<v0, v1, v2>`.
