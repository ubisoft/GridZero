// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG UNIT TESTS: MODEL IDENTITY, DISTINCT LEAF NAMES ACROSS NAMESPACES
// =============================================================================
//
// CRG_DECLARE_MODEL hashes the full qualified #ModelType (model_key.hpp), not
// a truncated leaf. Two distinct types sharing only their leaf name (e.g.
// studio::Widget and mod::Widget) hash to different values and must resolve
// to two distinct, independently-routable dense slots.
// =============================================================================

#include "catch.hpp"
#include "crg/crg.hpp"
#include "crg/core/dense_index.hpp"

namespace crg::test::collision {
    struct CollisionDomain {};
    namespace studio { struct Widget {}; }
    namespace mod    { struct Widget {}; }
}

CRG_DECLARE_DOMAIN(crg::test::collision::CollisionDomain)

CRG_DECLARE_DOMAIN_MODELS(crg::test::collision::CollisionDomain,
    crg::test::collision::studio::Widget,
    crg::test::collision::mod::Widget)

TEST_CASE("DenseIndexStore: distinct full names never collide across namespaces",
          "[models][store][collision]") {
    using Store = ::crg::core::DenseIndexStore<crg::test::collision::CollisionDomain>;

    const ::crg::u64 studioHash = ::crg::models::ModelKey<crg::test::collision::studio::Widget>::Value;
    const ::crg::u64 modHash    = ::crg::models::ModelKey<crg::test::collision::mod::Widget>::Value;
    REQUIRE(studioHash != modHash);

    Store::Refresh();
    const ::crg::u32 studioSlot = Store::Find(studioHash);
    const ::crg::u32 modSlot    = Store::Find(modHash);

    REQUIRE(studioSlot != ::crg::core::InvalidDenseSlot);
    REQUIRE(modSlot != ::crg::core::InvalidDenseSlot);
    REQUIRE(studioSlot != modSlot);
}
