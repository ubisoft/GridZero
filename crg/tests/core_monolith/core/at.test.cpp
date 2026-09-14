// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// UNIT TEST: COMPILE-TIME COORDINATE PROJECTION (MAKEAT)
// =============================================================================

#include "catch.hpp"
#include "crg/core/at.hpp"
#include <type_traits>

namespace {
    // =========================================================================
    // MOCK 3D CARTESIAN SPACE FOR COMPILE-TIME VERIFICATION
    // =========================================================================
    // Dimensions: Dim0 (Size 3, Stride 9), Dim1 (Size 3, Stride 3), Dim2 (Size 3, Stride 1)
    // Total Volume = 27
    struct Mock3DSpace {
        static constexpr std::size_t Dimensions = 3;
        static constexpr std::size_t Volume = 27;

        template<std::size_t Dim>
        static constexpr int GetCoordAtIndex(std::size_t index) {
            if constexpr (Dim == 0) {
                return static_cast<int>(index / 9);
            } else if constexpr (Dim == 1) {
                return static_cast<int>((index % 9) / 3);
            } else {
                return static_cast<int>(index % 3);
            }
        }
    };
}

// =============================================================================
// STATIC COMPILE-TIME TESTS (VALIDATING SFINAE IDENTITY)
// =============================================================================

TEST_CASE("Core: MakeAt Coordinate Projection", "[core][math]") {
    
    // Test Case: Index 0
    // Expected: 0/9 = 0, (0%9)/3 = 0, 0%3 = 0 -> At<0, 0, 0>
    STATIC_REQUIRE(std::is_same_v<
        ::crg::MakeAt<Mock3DSpace, 0>, 
        ::crg::At<0, 0, 0>
    >);

    // Test Case: Index 1
    // Expected: 1/9 = 0, (1%9)/3 = 0, 1%3 = 1 -> At<0, 0, 1>
    STATIC_REQUIRE(std::is_same_v<
        ::crg::MakeAt<Mock3DSpace, 1>, 
        ::crg::At<0, 0, 1>
    >);

    // Test Case: Index 9
    // Expected: 9/9 = 1, (9%9)/3 = 0, 9%3 = 0 -> At<1, 0, 0>
    STATIC_REQUIRE(std::is_same_v<
        ::crg::MakeAt<Mock3DSpace, 9>, 
        ::crg::At<1, 0, 0>
    >);

    // Test Case: Index 20 (Matches the robotics nominal performance test case)
    // Expected: 20/9 = 2, (20%9)/3 = 2/3 = 0, 20%3 = 2 -> At<2, 0, 2>
    STATIC_REQUIRE(std::is_same_v<
        ::crg::MakeAt<Mock3DSpace, 20>, 
        ::crg::At<2, 0, 2>
    >);

    // Test Case: Index 26 (Max boundaries)
    // Expected: 26/9 = 2, (26%9)/3 = 8/3 = 2, 26%3 = 2 -> At<2, 2, 2>
    STATIC_REQUIRE(std::is_same_v<
        ::crg::MakeAt<Mock3DSpace, 26>, 
        ::crg::At<2, 2, 2>
    >);
}
