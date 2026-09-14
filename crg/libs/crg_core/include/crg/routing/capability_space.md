# capability_space — Design Notes

## Purpose

`CapabilitySpace<TAxes...>` is the compile-time descriptor for an N-dimensional capability routing tensor. Each template axis is an enum type whose cardinality is read from `EnumTraits<TAxis>::Count`. The tensor's total `Volume` is the product of all axis counts.

## Tensor Offset Computation — Horner's Method

`ComputeOffset(modelIndex, coords...)` is the sole public entry point. It folds the dense model slot in as the outermost axis, then maps the N-tuple of enum coordinates to a flat 1D index using **Horner's method**:

```
offset = modelIndex * Volume + (...((c[0] * d[1] + c[1]) * d[2] + c[2])...)
```

This is a branchless, single-pass scan across dimensions. It is mathematically equivalent to row-major (last-axis-fastest) linearization but avoids computing individual strides per dimension. The implementation is `constexpr` and evaluates fully at compile time when `modelIndex` and coordinates are all compile-time constants.

There is no public "pure geometry" overload (`ComputeOffset(coords...)` without a model). Nothing in CRG needs an offset that isn't scoped to a model — `Find()` always has a handle — so the coordinate-only Horner reduction (`ComputeOffsetWithinModel`) stays `private`, called only as the base case. Tests that want to isolate the pure axis math pass `modelIndex=0`.

## Stride and Inverse Mapping

`GetStride<TDimIdx>()` returns the stride of dimension `TDimIdx` in the flat layout (product of all axis sizes with higher indices). `GetCoordAtIndex<TDimIdx>(index)` inverts the flat index to recover the enum value along one axis: `(index / stride) % axisCount`.

## `AtType<TIndex>` — Compile-Time Coordinate Extraction

`AtType<TIndex>` is an alias for `::crg::At<coords...>` where each coordinate is the compile-time enum value along the corresponding axis at flat index `TIndex`. This is used by `::crg::MakeAt` to reconstruct the typed coordinate pack from a flat slot index, enabling static dispatch back into capability bindings.

## Edge Cases

- **0 dimensions (`Dimensions == 0`):** `Volume = 1`, all offset computations return 0, and `AtType<0>` is `::crg::At<>`. This models the degenerate scalar (0D) tensor used by unconditional capability bindings.
