// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// UNIT TEST: SCOPED NO-ALLOC GUARD
// =============================================================================
//
// IMPORTANT: All REQUIRE calls must be placed OUTSIDE ScopedNoAlloc scopes.
// Catch2's REQUIRE macro allocates heap to record assertion results — triggering
// the exact abort we are testing. Pattern: capture bools inside the guard,
// assert the bools outside.

#include "catch.hpp"
#include "crg/core/scoped_no_alloc.hpp"
#include <thread>
#include <atomic>
#include <type_traits>

using crg::core::ScopedNoAlloc;

TEST_CASE("ScopedNoAlloc flag lifecycle", "[core][no_alloc]") {
    bool beforeActive = ScopedNoAlloc::IsActive();
    bool duringActive = false;
    {
        ScopedNoAlloc guard;
        duringActive = ScopedNoAlloc::IsActive();
        // no REQUIRE here — Catch2 allocation would abort
    }
    bool afterActive = ScopedNoAlloc::IsActive();

    REQUIRE_FALSE(beforeActive);
    REQUIRE(duringActive);
    REQUIRE_FALSE(afterActive);
}

TEST_CASE("ScopedNoAlloc thread isolation", "[core][no_alloc]") {
    // std::thread constructor allocates heap — create it BEFORE the guard activates.
    // Use std::atomic (no heap) for synchronization inside the guard scope.
    bool threadSawActive = true;  // pessimistic default
    std::atomic<bool> start{false};
    std::atomic<bool> checked{false};

    std::thread t([&start, &checked, &threadSawActive]() {
        while (!start.load(std::memory_order_acquire)) {}
        // This thread has its own thread_local g_NoAllocDepth (0); the main
        // thread's guard must NOT be visible here.
        threadSawActive = ScopedNoAlloc::IsActive();
        checked.store(true, std::memory_order_release);
    });

    bool mainSawActive = false;
    {
        ScopedNoAlloc guard;
        mainSawActive = ScopedNoAlloc::IsActive();
        start.store(true, std::memory_order_release);
        while (!checked.load(std::memory_order_acquire)) {}
        // no REQUIRE inside guard
    }

    t.join();

    REQUIRE(mainSawActive);
    REQUIRE_FALSE(threadSawActive);
}

TEST_CASE("ScopedNoAlloc is not copy-constructible or copy-assignable", "[core][no_alloc]") {
    STATIC_REQUIRE(!std::is_copy_constructible_v<ScopedNoAlloc>);
    STATIC_REQUIRE(!std::is_copy_assignable_v<ScopedNoAlloc>);
}

TEST_CASE("ScopedNoAlloc nesting keeps zone active until last guard destroyed", "[core][no_alloc]") {
    bool afterInner  = false;
    bool duringOuter = false;
    bool duringInner = false;
    {
        ScopedNoAlloc outer;
        duringOuter = ScopedNoAlloc::IsActive();
        {
            ScopedNoAlloc inner;
            duringInner = ScopedNoAlloc::IsActive();
        }
        afterInner = ScopedNoAlloc::IsActive();  // outer still alive — depth 1
    }
    bool afterOuter = ScopedNoAlloc::IsActive();

    REQUIRE(duringOuter);
    REQUIRE(duringInner);
    REQUIRE(afterInner);    // outer still alive after inner destroyed
    REQUIRE_FALSE(afterOuter);
}

TEST_CASE("ScopedNoAlloc IsActive matches guard scope via helper", "[core][no_alloc]") {
    auto checkActive = []() noexcept -> bool { return ScopedNoAlloc::IsActive(); };

    bool before = checkActive();
    bool during = false;
    {
        ScopedNoAlloc guard;
        during = checkActive();
    }
    bool after = checkActive();

    REQUIRE_FALSE(before);
    REQUIRE(during);
    REQUIRE_FALSE(after);
}
