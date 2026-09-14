// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// =============================================================================
// CRG UNIT TESTS: DENSE INDEX STORE (NodeLink-fed feeder contract)
// =============================================================================
//
// Covers the post-Intern feeder model:
//   * NodeLink-driven discovery — ModelNode<Domain, Hash> instances inject
//     entries into the per-domain chain at static init.
//   * Refresh walks the chain, dedup'd by hash, append-only.
//   * Find is read-only and lock-free; unknown hashes get InvalidDenseSlot.
//   * ModelToken::FromType / FromKey / FromHash are Find-only.
// =============================================================================

#include "catch.hpp"
#include "crg/crg.hpp"
#include "crg/core/dense_index.hpp"
#include "crg/core/hash.hpp"
#include "crg/models/dense_id.hpp"
#include "crg/models/model_token.hpp"
#include "crg/models/model_node.hpp"

namespace crg::test::store {
    struct StoreDomainA {};
    struct StoreDomainB {};

    struct ModelAlpha {};
    struct ModelBeta  {};
    struct ModelGamma {};
    struct ModelDelta {};  // belongs to StoreDomainB only
}

CRG_DECLARE_DOMAIN(crg::test::store::StoreDomainA)
CRG_DECLARE_DOMAIN(crg::test::store::StoreDomainB)

CRG_DECLARE_DOMAIN_MODELS(crg::test::store::StoreDomainA,
    crg::test::store::ModelAlpha,
    crg::test::store::ModelBeta,
    crg::test::store::ModelGamma)

// StoreDomainB owns ModelDelta only — models belong to exactly one domain.
CRG_DECLARE_DOMAIN_MODELS(crg::test::store::StoreDomainB,
    crg::test::store::ModelDelta)

TEST_CASE("DenseIndexStore: dedup", "[models][store][dedup]") {
    using namespace crg::test::store;
    using Store = ::crg::core::DenseIndexStore<StoreDomainA>;

    Store::Refresh();

    // Three unique hashes regardless of duplicate CRG_DECLARE_DOMAIN_MODELS.
    REQUIRE(Store::Size() == 3u);
}

TEST_CASE("DenseIndexStore: idempotent refresh", "[models][store][refresh]") {
    using namespace crg::test::store;
    using Store = ::crg::core::DenseIndexStore<StoreDomainA>;

    Store::Refresh();
    const ::crg::u32 sizeAfterFirst = Store::Size();
    const ::crg::u32 slotAlpha = Store::Find(::crg::hash::TypeHash<ModelAlpha>::Value);

    Store::Refresh();
    REQUIRE(Store::Size() == sizeAfterFirst);
    REQUIRE(Store::Find(::crg::hash::TypeHash<ModelAlpha>::Value) == slotAlpha);
}

TEST_CASE("DenseIndexStore: monotone slots across refreshes", "[models][store][monotone]") {
    using namespace crg::test::store;
    using Store = ::crg::core::DenseIndexStore<StoreDomainA>;

    Store::Refresh();
    const ::crg::u32 slotAlpha = Store::Find(::crg::hash::TypeHash<ModelAlpha>::Value);
    const ::crg::u32 slotBeta  = Store::Find(::crg::hash::TypeHash<ModelBeta >::Value);
    const ::crg::u32 slotGamma = Store::Find(::crg::hash::TypeHash<ModelGamma>::Value);

    REQUIRE(slotAlpha != ::crg::core::InvalidDenseSlot);
    REQUIRE(slotBeta  != ::crg::core::InvalidDenseSlot);
    REQUIRE(slotGamma != ::crg::core::InvalidDenseSlot);
    REQUIRE(slotAlpha != slotBeta);
    REQUIRE(slotAlpha != slotGamma);
    REQUIRE(slotBeta  != slotGamma);

    // Refresh a second time — already-published slots must not move.
    Store::Refresh();
    REQUIRE(Store::Find(::crg::hash::TypeHash<ModelAlpha>::Value) == slotAlpha);
    REQUIRE(Store::Find(::crg::hash::TypeHash<ModelBeta >::Value) == slotBeta);
    REQUIRE(Store::Find(::crg::hash::TypeHash<ModelGamma>::Value) == slotGamma);
}

TEST_CASE("DenseIndexStore: domain isolation", "[models][store][isolation]") {
    using namespace crg::test::store;
    using StoreA = ::crg::core::DenseIndexStore<StoreDomainA>;
    using StoreB = ::crg::core::DenseIndexStore<StoreDomainB>;

    StoreA::Refresh();
    StoreB::Refresh();

    // StoreDomainA owns Alpha/Beta/Gamma; StoreDomainB owns Delta only.
    // Each model belongs to exactly one domain — no cross-registration.
    REQUIRE(StoreA::Find(::crg::hash::TypeHash<ModelAlpha>::Value) != ::crg::core::InvalidDenseSlot);
    REQUIRE(StoreA::Find(::crg::hash::TypeHash<ModelBeta>::Value)  != ::crg::core::InvalidDenseSlot);
    REQUIRE(StoreA::Find(::crg::hash::TypeHash<ModelGamma>::Value) != ::crg::core::InvalidDenseSlot);

    // ModelDelta is registered in StoreDomainB; Alpha, Beta, Gamma are not.
    REQUIRE(StoreB::Find(::crg::hash::TypeHash<ModelDelta>::Value) != ::crg::core::InvalidDenseSlot);
    REQUIRE(StoreB::Find(::crg::hash::TypeHash<ModelAlpha>::Value) == ::crg::core::InvalidDenseSlot);
    REQUIRE(StoreB::Find(::crg::hash::TypeHash<ModelGamma>::Value) == ::crg::core::InvalidDenseSlot);

    // StoreB has exactly one entry.
    REQUIRE(StoreB::Size() == 1u);
}

TEST_CASE("DenseIndexStore: unknown hash is non-mutating", "[models][store][readonly]") {
    using namespace crg::test::store;
    using Store = ::crg::core::DenseIndexStore<StoreDomainA>;

    Store::Refresh();
    const ::crg::u32 sizeBefore = Store::Size();

    constexpr ::crg::u64 unknownHash = 0xDEADBEEFCAFEBABEULL;
    REQUIRE(Store::Find(unknownHash) == ::crg::core::InvalidDenseSlot);

    // The Find call must not have grown the store — this is the load-bearing
    // assertion that proves GetOrCreate is dead.
    REQUIRE(Store::Size() == sizeBefore);
}

TEST_CASE("ModelToken: FromKey is Find-only", "[models][token][find]") {
    using namespace crg::test::store;
    using namespace crg::models;
    using Store = ::crg::core::DenseIndexStore<StoreDomainA>;

    Store::Refresh();
    const ::crg::u32 sizeBefore = Store::Size();

    // Known hash → valid token.
    auto h1 = ModelToken<StoreDomainA>::FromKey(::crg::hash::TypeHash<ModelAlpha>::Value);
    REQUIRE(h1.IsValid());

    // Unknown hash → invalid token, store untouched.
    auto h2 = ModelToken<StoreDomainA>::FromKey(0xBADBADBAD);
    REQUIRE_FALSE(h2.IsValid());
    REQUIRE(Store::Size() == sizeBefore);
}

TEST_CASE("ModelToken: FromHash alias parity", "[models][token][hash]") {
    using namespace crg::test::store;
    using namespace crg::models;
    using Store = ::crg::core::DenseIndexStore<StoreDomainA>;

    Store::Refresh();
    constexpr ::crg::u64 hash = ::crg::hash::TypeHash<ModelBeta>::Value;
    auto byKey  = ModelToken<StoreDomainA>::FromKey(hash);
    auto byHash = ModelToken<StoreDomainA>::FromHash(hash);
    REQUIRE(byKey.IsValid());
    REQUIRE(byHash.IsValid());
    REQUIRE(byKey.GetDenseIndex() == byHash.GetDenseIndex());
}

TEST_CASE("ModelToken: FromType matches FromKey on registered model", "[models][token][type]") {
    using namespace crg::test::store;
    using namespace crg::models;
    using Store = ::crg::core::DenseIndexStore<StoreDomainA>;

    Store::Refresh();
    auto fromType = ModelToken<StoreDomainA>::FromType<ModelGamma>();
    auto fromKey  = ModelToken<StoreDomainA>::FromKey(::crg::hash::TypeHash<ModelGamma>::Value);
    REQUIRE(fromType.IsValid());
    REQUIRE(fromKey.IsValid());
    REQUIRE(fromType.GetDenseIndex() == fromKey.GetDenseIndex());
}
