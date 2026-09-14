# scoped_no_alloc — design notes

## Purpose

`ScopedNoAlloc` marks the current thread as being inside a no-allocation zone.
It is used to instrument analytic hot-paths where a heap allocation indicates a correctness bug — typically meaning `RefreshCache()` was not called and the cache is not warm.

## Usage contract for `CRG_INSTALL_NO_ALLOC_GUARD()`

Place the macro exactly **once** in a single `.cpp` of the host executable (e.g. `main_runner.cpp`).
It installs global `operator new` / `operator delete` overrides that call `std::abort()` if an allocation is attempted while `ScopedNoAlloc::IsActive()` is true on the calling thread.

Do **not** place it in a header or in more than one translation unit — the linker will reject duplicate `operator new` definitions.

## Nesting

The guard is nestable: the zone stays active until the outermost `ScopedNoAlloc` is destroyed.
The depth counter is `int` (not `bool`) so that nested scopes compose correctly without spuriously deactivating the zone.
The counter is `thread_local` — no contention, no atomics required.
