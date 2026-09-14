// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// scoped_no_alloc.hpp — RAII guard for thread-scoped no-allocation zones

#pragma once
#include <cstdlib>

namespace crg::core {

namespace internal {
    // int (not bool): nested guards compose — depth > 0 keeps the zone active.
    inline thread_local int g_NoAllocDepth = 0;
}

struct ScopedNoAlloc {
    ScopedNoAlloc()  noexcept { ++internal::g_NoAllocDepth; }
    ~ScopedNoAlloc() noexcept { --internal::g_NoAllocDepth; }
    ScopedNoAlloc(const ScopedNoAlloc&) = delete;
    ScopedNoAlloc& operator=(const ScopedNoAlloc&) = delete;
    static bool IsActive() noexcept { return internal::g_NoAllocDepth > 0; }
};

} // namespace crg::core

#define CRG_INSTALL_NO_ALLOC_GUARD()                                                  \
    void* operator new(std::size_t sz) {                                              \
        if (::crg::core::internal::g_NoAllocDepth > 0) { std::abort(); }             \
        void* p = std::malloc(sz);                                                    \
        if (!p) std::abort();                                                         \
        return p;                                                                     \
    }                                                                                 \
    void* operator new[](std::size_t sz) {                                            \
        if (::crg::core::internal::g_NoAllocDepth > 0) { std::abort(); }             \
        void* p = std::malloc(sz);                                                    \
        if (!p) std::abort();                                                         \
        return p;                                                                     \
    }                                                                                 \
    void operator delete(void* p)                noexcept { std::free(p); }          \
    void operator delete[](void* p)              noexcept { std::free(p); }          \
    /* size-aware overloads required: partial replacement sets cause linker warnings on modern compilers (C++14) */ \
    void operator delete(void* p, std::size_t)   noexcept { std::free(p); }          \
    void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
