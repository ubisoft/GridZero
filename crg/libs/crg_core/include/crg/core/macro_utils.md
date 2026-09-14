# macro_utils — Design Notes

## Purpose

Provides `CRG_FOR_EACH(M, ...)` — applies a one-argument macro `M` to each element of a variadic list.

## MSVC Compatibility

MSVC's legacy preprocessor (`/Zc:preprocessor` off) expands `__VA_ARGS__` as a single token. A naive `M(__VA_ARGS__)` produces `M(a,b,c)` rather than `M(a) M(b) M(c)`. The `CRG_EXPAND` macro forces a rescan pass, which makes the argument splitting work correctly under both the legacy and conforming (`/Zc:preprocessor`) preprocessors.

`CRG_GET_NTH_ARG` uses the standard argument-counting trick: the user's arguments push the numeric sentinel sequence rightward, and the 17th position always lands on the correct `CRG_FE_N` selector.

## Capacity

Current limit: **16 arguments**. To extend, add `CRG_FE_17`, `CRG_FE_18`, … and extend the argument list in `CRG_FOR_EACH` accordingly.

## Usage

```cpp
#define DECLARE(x) struct x;
CRG_FOR_EACH(DECLARE, A, B, C)   // expands to: struct A; struct B; struct C;
```
