// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// ManagedModel patching — routing layer test (zero consumer include)
// ─────────────────────────────────────────────────────────────────────────────
// Validates the core insight of managed-object patching:
//   TypeHash<ManagedModel<ClassHash, ObjectID>>::Value == ObjectID
//   → the tensor cell at ObjectID IS the patch, no extra registry.
//
// Uses a Brain capability (virtual span accessors) — the "passive data" shape.
// No consumer types, no object-model include. Only CRG primitives.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

// ─── Domain ──────────────────────────────────────────────────────────────────

struct PatchTestDomain {};
CRG_DECLARE_DOMAIN(PatchTestDomain)
CRG_DEFINE_DOMAIN(PatchTestDomain)

// ─── ManagedModel — the whole trick ──────────────────────────────────────────
// TypeHash::Value == ObjectID → tensor cell at ObjectID is the patch.

template<u64 ClassHash, u64 ObjectID>
struct ManagedModel {
    static constexpr u64 ClassHashValue = ClassHash;
    static constexpr u64 ObjectIDValue  = ObjectID;
};

namespace crg::hash {
    template<u64 C, u64 O>
    struct TypeHash<::ManagedModel<C, O>> {
        static constexpr u64 Value = O;
    };
}
namespace crg::models {
    template<u64 C, u64 O>
    struct ModelKey<::ManagedModel<C, O>>
        : public ::crg::hash::TypeHash<::ManagedModel<C, O>> {};
}

// ─── Minimal patch capability (Brain) ────────────────────────────────────────

struct IPatchData {
    virtual const float* GetFloats(u32& count) const noexcept = 0;
    virtual ~IPatchData() = default;
};

template<class TModel>
struct BikeSpeedPatch : Capability<IPatchData> {
    static constexpr float Floats_[] = { 85.0f, 12.5f };
    const float* GetFloats(u32& count) const noexcept override {
        count = 2;
        return Floats_;
    }
};

// ─── Binding — ObjectID 42, ClassHash 0xDEADC0DEu ────────────────────────────

using BikeEntry = ::ManagedModel<0xDEADC0DEu, 42ULL>;

// Full specialization (more specialized than the generic template above) —
// only BikeEntry is ever registered via CRG_DECLARE_DOMAIN_MODELS below, so
// only it needs a Name matching that macro's #ModelType text. Giving the
// generic template a hardcoded Name would be wrong for every other
// ManagedModel<C, O> instantiation (e.g. BikeEntry2 below).
namespace crg::hash {
    template<>
    struct TypeHash<::BikeEntry> {
        static constexpr u64 Value = 42ULL;
        CRG_HASH_NAME_ENABLED_ONLY(static constexpr std::string_view Name = "BikeEntry";)
    };
}

CRG_DECLARE_DOMAIN_MODELS(PatchTestDomain, BikeEntry)

namespace {
    static const CapabilityBinding<PatchTestDomain, BikeEntry, BikeSpeedPatch> s_Binding{};
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST_CASE("ManagedModel — TypeHash::Value equals ObjectID", "[patching][managed_model]") {
    STATIC_REQUIRE(crg::hash::TypeHash<BikeEntry>::Value == 42ULL);
}

TEST_CASE("ManagedModel — Find returns valid handle at ObjectID", "[patching][managed_model]") {
    auto token = ModelToken<PatchTestDomain>::FromKey(42ULL);
    auto gate  = CapabilityRouter<PatchTestDomain>::Find<IPatchData>(token);
    REQUIRE(gate);
}

TEST_CASE("ManagedModel — Find returns null for unknown ObjectID", "[patching][managed_model]") {
    auto token = ModelToken<PatchTestDomain>::FromKey(99ULL);
    auto gate  = CapabilityRouter<PatchTestDomain>::Find<IPatchData>(token);
    REQUIRE_FALSE(gate);
}

TEST_CASE("ManagedModel — span data accessible through Brain handle", "[patching][managed_model]") {
    auto token = ModelToken<PatchTestDomain>::FromKey(42ULL);
    auto gate  = CapabilityRouter<PatchTestDomain>::Find<IPatchData>(token);
    REQUIRE(gate);

    u32 count = 0;
    const float* floats = gate->GetFloats(count);
    REQUIRE(count == 2);
    REQUIRE(floats[0] == 85.0f);
    REQUIRE(floats[1] == 12.5f);
}

TEST_CASE("ManagedModel — ClassHash is independent of ObjectID", "[patching][managed_model]") {
    // Two entries with same ClassHash but different ObjectID are distinct cells.
    using BikeEntry2 = ::ManagedModel<0xDEADC0DEu, 99ULL>;
    STATIC_REQUIRE(crg::hash::TypeHash<BikeEntry2>::Value == 99ULL);
    STATIC_REQUIRE(crg::hash::TypeHash<BikeEntry>::Value  == 42ULL);
}
