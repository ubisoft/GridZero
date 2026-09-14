// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG UNIT TESTS (CATCH2) - PARTITION & MODEL IDENTITY
// =============================================================================

#include "catch.hpp"
#include "crg/crg_models.hpp"
#include "crg/core/types.hpp"
#include "crg/core/hash.hpp"

using namespace crg;
using namespace crg::models;

// =============================================================================
// 1. DATA STRUCTURES
// =============================================================================
namespace crg::models::test {
    struct SensorUpdate {};
    struct RoboticsPartition {};
}

// =============================================================================
// 2. STRICT REGISTRATION
// =============================================================================
CRG_DECLARE_DOMAIN(crg::models::test::RoboticsPartition)
CRG_DECLARE_DOMAIN_MODELS(crg::models::test::RoboticsPartition,
    crg::models::test::SensorUpdate)

// =============================================================================
// 3. TEST SUITES
// =============================================================================
namespace crg::models::test {

    TEST_CASE("Domain Identity: Governance & Traits", "[models][domain]") {
        
        SECTION("Domain Topology (Identity)") {
            constexpr u64 expectedHash = ::crg::hash::HashString("crg::models::test::RoboticsPartition");
            
            // Evaluated 100% at compile-time. Zero runtime cost.
            STATIC_REQUIRE(DomainTraits<RoboticsPartition>::Identity == expectedHash);
        }

        SECTION("Model Identity (TypeKey)") {
            // CRG_DECLARE_MODEL hashes the full qualified type name, same
            // convention as CRG_DECLARE_HASH for domains/contracts.
            constexpr u64 expectedHash = ::crg::hash::HashString("crg::models::test::SensorUpdate");

            // Evaluated 100% at compile-time. Zero runtime cost.
            STATIC_REQUIRE(DomainTraits<RoboticsPartition>::template TypeKey<SensorUpdate> == expectedHash);
        }
    }
}
