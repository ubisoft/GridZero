// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// STAGE 01 — Models: Typed Entity Handles (DenseID + ModelToken)
// ─────────────────────────────────────────────────────────────────────────────
// PROBLEM:
//   Routing a dispatch to the right function for a given entity type requires
//   knowing the type at runtime. A vtable works, but it chains through a heap
//   pointer on every call and scatters the data across memory.
//
// SOLUTION:
//   DenseID. Each registered model type gets a unique, small integer index
//   (0, 1, 2…) assigned at first use via ModelMapper. ModelToken<TDomain>
//   wraps one such index. The routing tensor uses it as a row offset:
//   one flat array lookup, no pointer chasing.
//
// WHEN TO USE (without going further):
//   - You only need stable, compact per-type identity (e.g., for type-keyed maps)
//   - You're building the routing infrastructure but not dispatching yet
//
// WHAT'S NEW vs Stage 00:
//   Domain (CRG_DECLARE_DOMAIN), model registration (CRG_DECLARE_MODEL),
//   ModelToken. No capability or routing tensor yet.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg::models;

// ─── Domain — an isolated routing universe ───────────────────────────────────
//   A domain is an empty tag struct. All types registered under it (models,
//   capabilities, routing traits) are completely isolated from other domains.
//   Different domains can reuse the same model or contract types — no conflict.
struct S01Domain {};
CRG_DECLARE_DOMAIN(S01Domain)
CRG_DEFINE_DOMAIN(S01Domain)   // empty in monolithic mode; active in DLL mode

// ─── Entity types ─────────────────────────────────────────────────────────────
//   CRG_DECLARE_DOMAIN_MODELS registers each model's FNV-1a hash, makes
//   ModelKey<T> complete (otherwise FromType<T>() fails to compile), and
//   specializes ModelDomain<T>::Type = S01Domain (consumed by PipelineCapability).
//   The static_assert inside the macro enforces that S01Domain was declared
//   first via CRG_DECLARE_DOMAIN / CRG_DECLARE_PIPELINE_DOMAIN.
struct S01Robot {};
struct S01Drone {};
CRG_DECLARE_DOMAIN_MODELS(S01Domain,
    S01Robot,
    S01Drone)

// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Stage 01 — ModelToken carries a valid DenseID", "[stage01][models]") {
    auto robot = ModelToken<S01Domain>::FromType<S01Robot>();
    auto drone = ModelToken<S01Domain>::FromType<S01Drone>();

    REQUIRE(robot.IsValid());
    REQUIRE(drone.IsValid());
}

TEST_CASE("Stage 01 — Dense IDs are distinct and contiguous", "[stage01][models]") {
    auto robot = ModelToken<S01Domain>::FromType<S01Robot>();
    auto drone = ModelToken<S01Domain>::FromType<S01Drone>();

    // Different types → different slots.
    // Values are small non-negative integers (0, 1, …), enabling flat array indexing.
    REQUIRE(robot.GetDenseIndex() != drone.GetDenseIndex());
    REQUIRE(robot.GetDenseIndex() < 2);
    REQUIRE(drone.GetDenseIndex() < 2);
}

TEST_CASE("Stage 01 — FromType is idempotent", "[stage01][models]") {
    // Calling FromType<T>() twice yields the same slot — no double-registration.
    auto h1 = ModelToken<S01Domain>::FromType<S01Robot>();
    auto h2 = ModelToken<S01Domain>::FromType<S01Robot>();
    REQUIRE(h1.GetDenseIndex() == h2.GetDenseIndex());
}
