// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// UNIT TEST: CountTrailingZeros64 — portable C++17 TZCNT
// =============================================================================
// Replaces the previous compiler-intrinsic-gated implementation. Verifies the
// pure binary-search version returns identical bit indices across the full
// 64-bit range, including the edge cases the intrinsic path used to handle
// (lowest bit, highest bit, sparse masks).
// =============================================================================

#include "catch.hpp"
#include "crg/routing/branchless_dispatch_cell.hpp"

#include <cstdint>

using crg::routing::CountTrailingZeros64;

TEST_CASE("CountTrailingZeros64: single bit set at every position", "[routing][bits]") {
    for (uint32_t k = 0; k < 64; ++k) {
        const uint64_t mask = uint64_t{1} << k;
        REQUIRE(CountTrailingZeros64(mask) == k);
    }
}

TEST_CASE("CountTrailingZeros64: lowest bit dominates over higher bits", "[routing][bits]") {
    // The function returns the index of the LOWEST set bit, regardless of
    // higher bits. Probe the relation explicitly across the whole range.
    for (uint32_t k = 0; k < 63; ++k) {
        const uint64_t low  = uint64_t{1} << k;
        const uint64_t high = uint64_t{1} << 63;
        REQUIRE(CountTrailingZeros64(low | high) == k);
    }
}

TEST_CASE("CountTrailingZeros64: sparse and dense masks", "[routing][bits]") {
    REQUIRE(CountTrailingZeros64(0xFFFFFFFFFFFFFFFFULL) == 0u);  // every bit set → 0
    REQUIRE(CountTrailingZeros64(0x8000000000000000ULL) == 63u); // only top bit
    REQUIRE(CountTrailingZeros64(0x0000000100000000ULL) == 32u); // mid boundary
    REQUIRE(CountTrailingZeros64(0x0000000000010000ULL) == 16u);
    REQUIRE(CountTrailingZeros64(0x0000000000000100ULL) == 8u);
    REQUIRE(CountTrailingZeros64(0xAAAAAAAAAAAAAAAAULL) == 1u);  // 0b...10 → bit 1
    REQUIRE(CountTrailingZeros64(0x5555555555555554ULL) == 2u);  // 0b...0100 → bit 2
}
