// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG UNIT TESTS: DENSE ID & MODEL TOKEN (Find-only contract)
// =============================================================================
//
// The old GetOrCreate path is gone — ModelToken::FromType<T>() and
// FromKey(hash) are pure Find against DenseIndexStore<TDomain>. Slots are fed
// exclusively by ModelNodeBase<TDomain> chain entries emitted by
// CRG_DECLARE_DOMAIN_MODELS, walked by DenseIndexStore::Refresh.
// =============================================================================

#include "catch.hpp"
#include "crg/crg.hpp"
#include "crg/core/dense_index.hpp"
#include "crg/models/dense_id.hpp"
#include "crg/models/model_token.hpp"
#include "crg/models/domain_traits.hpp"

namespace crg::test::ids {
    struct IDPartitionA {};
    struct IDPartitionB {};

    struct MockModelX {};
    struct MockModelY {};
    struct MockModelZ {};
}

CRG_DECLARE_DOMAIN(crg::test::ids::IDPartitionA)
CRG_DECLARE_DOMAIN(crg::test::ids::IDPartitionB)
CRG_DECLARE_DOMAIN_MODELS(crg::test::ids::IDPartitionA,
    crg::test::ids::MockModelX,
    crg::test::ids::MockModelY)
CRG_DECLARE_DOMAIN_MODELS(crg::test::ids::IDPartitionB,
    crg::test::ids::MockModelZ)

TEST_CASE("Models: DenseID and ModelToken Integrity", "[models][id]") {
    using namespace crg::test::ids;
    using namespace crg::models;
    using StoreA = ::crg::core::DenseIndexStore<IDPartitionA>;
    using StoreB = ::crg::core::DenseIndexStore<IDPartitionB>;

    // Refresh both stores so the NodeLink-fed chains are linearized into
    // hash → slot directories before any FromType/FromKey lookup.
    StoreA::Refresh();
    StoreB::Refresh();

    SECTION("1. Deterministic Allocation (Idempotency)") {
        auto token1 = ModelToken<IDPartitionA>::FromType<MockModelX>();
        auto token2 = ModelToken<IDPartitionA>::FromType<MockModelX>();

        REQUIRE(token1.IsValid());
        // Two FromType calls on the same model must yield the same slot —
        // Find is deterministic.
        REQUIRE(token1.GetDenseIndex() == token2.GetDenseIndex());
    }

    SECTION("2. Distinct Models Get Distinct Slots") {
        auto tokenX = ModelToken<IDPartitionA>::FromType<MockModelX>();
        auto tokenY = ModelToken<IDPartitionA>::FromType<MockModelY>();

        REQUIRE(tokenX.IsValid());
        REQUIRE(tokenY.IsValid());
        REQUIRE(tokenX.GetDenseIndex() != tokenY.GetDenseIndex());
    }

    SECTION("3. Domain Isolation (Zero Cross-Talk)") {
        // Models belong to exactly one domain. Each store is independent.
        // MockModelX/Y live in A; MockModelZ lives in B.
        auto tokenA = ModelToken<IDPartitionA>::FromType<MockModelX>();
        auto tokenB = ModelToken<IDPartitionB>::FromType<MockModelZ>();

        REQUIRE(tokenA.IsValid());
        REQUIRE(tokenB.IsValid());

        // B does not know MockModelX (it belongs to A only).
        auto crossLookup = ModelToken<IDPartitionB>::FromType<MockModelX>();
        REQUIRE_FALSE(crossLookup.IsValid());
    }
}
