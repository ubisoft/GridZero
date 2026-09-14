// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// UNIT TEST: CAPABILITY SPACE (MATH & TOPOLOGY)
// =============================================================================

#include "catch.hpp"
#include "crg/routing/capability_space.hpp"
#include "crg/core/enum_traits.hpp"

namespace {
    // We strictly use asymmetric dimensions to validate math formulas and prevent
    // axis-inversion false positives.
    enum class DimA { A0, A1 };             // Count: 2
    enum class DimB { B0, B1, B2 };         // Count: 3
    enum class DimC { C0, C1, C2, C3 };     // Count: 4
}

namespace crg {
    template<> struct EnumTraits<DimA> { static constexpr std::size_t Count = 2; };
    template<> struct EnumTraits<DimB> { static constexpr std::size_t Count = 3; };
    template<> struct EnumTraits<DimC> { static constexpr std::size_t Count = 4; };
}

TEST_CASE("Routing: CapabilitySpace Math Topology", "[routing][math]") {
    using Space = ::crg::routing::CapabilitySpace<DimA, DimB, DimC>;

    SECTION("Compile-Time: Dimensions and Volume") {
        STATIC_REQUIRE(Space::Dimensions == 3);
        
        // Volume = 2 * 3 * 4 = 24
        STATIC_REQUIRE(Space::Volume == 24); 
    }

    SECTION("Compile-Time: Strides Calculation") {
        // Stride C (Dim 2) = 1 (always 1 for the innermost dimension)
        // Stride B (Dim 1) = Count(C) = 4
        // Stride A (Dim 0) = Count(B) * Count(C) = 12
        STATIC_REQUIRE(Space::GetStride<2>() == 1);
        STATIC_REQUIRE(Space::GetStride<1>() == 4);
        STATIC_REQUIRE(Space::GetStride<0>() == 12);
    }

    SECTION("Compile-Time: Forward Projection (ComputeOffset)") {
        // modelIndex=0 isolates the pure axis geometry (0 * Volume + offset == offset).
        // Target: A1 (val: 1), B2 (val: 2), C1 (val: 1)
        // Offset = (1 * 12) + (2 * 4) + (1 * 1) = 12 + 8 + 1 = 21
        STATIC_REQUIRE(Space::ComputeOffset(0, DimA::A1, DimB::B2, DimC::C1) == 21);

        // Target: A0 (0), B0 (0), C0 (0) -> Origin
        STATIC_REQUIRE(Space::ComputeOffset(0, DimA::A0, DimB::B0, DimC::C0) == 0);

        // Target: A1 (1), B2 (2), C3 (3) -> Maximum boundaries
        // Offset = (1 * 12) + (2 * 4) + (3 * 1) = 12 + 8 + 3 = 23 (Volume - 1)
        STATIC_REQUIRE(Space::ComputeOffset(0, DimA::A1, DimB::B2, DimC::C3) == 23);
    }

    SECTION("Compile-Time: Model Slot Folded as Outermost Axis") {
        // modelIndex * Volume + offset — Volume = 24
        STATIC_REQUIRE(Space::ComputeOffset(2, DimA::A1, DimB::B2, DimC::C1) == 2 * 24 + 21);
        STATIC_REQUIRE(Space::ComputeOffset(5, DimA::A0, DimB::B0, DimC::C0) == 5 * 24);
    }

    SECTION("Compile-Time: Reverse Projection (GetCoordAtIndex)") {
        // Test Index 21 -> Expected: A1, B2, C1
        STATIC_REQUIRE(Space::GetCoordAtIndex<0>(21) == DimA::A1);
        STATIC_REQUIRE(Space::GetCoordAtIndex<1>(21) == DimB::B2);
        STATIC_REQUIRE(Space::GetCoordAtIndex<2>(21) == DimC::C1);
        
        // Test Index 0 -> Expected: Origin
        STATIC_REQUIRE(Space::GetCoordAtIndex<0>(0) == DimA::A0);
        STATIC_REQUIRE(Space::GetCoordAtIndex<1>(0) == DimB::B0);
        STATIC_REQUIRE(Space::GetCoordAtIndex<2>(0) == DimC::C0);
    }

    SECTION("Compile-Time: Zero-Dimension (Empty Space) Guard") {
        // Validates that capabilities without contextual arguments don't break the router
        using EmptySpace = ::crg::routing::CapabilitySpace<>;
        
        STATIC_REQUIRE(EmptySpace::Dimensions == 0);
        STATIC_REQUIRE(EmptySpace::Volume == 1); // Minimum 1 cell for baseline logic/fallback
        STATIC_REQUIRE(EmptySpace::GetStride<0>() == 1);
        STATIC_REQUIRE(EmptySpace::ComputeOffset(0) == 0);
        STATIC_REQUIRE(EmptySpace::ComputeOffset(3) == 3); // Volume == 1, so modelIndex passes through
    }
}
