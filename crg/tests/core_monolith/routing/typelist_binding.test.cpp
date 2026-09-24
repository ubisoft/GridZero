// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// TYPELIST BINDING — CapabilityBinding<TDomain, TypeList<TModels...>, TCaps...>
// ─────────────────────────────────────────────────────────────────────────────
// CapabilityBinding's TModel slot accepts either a single model or a
// crg::TypeList<TModels...> of models, fanning the same capability set across
// every listed model from one declaration (see capability_binding.md,
// "TModel = TypeList<TModels...>").
//
// Mechanism: CapabilityBinding<TDomain, TypeList<TModels...>, TCaps...> derives
// from CapabilityBinding<TDomain, TModels, TCaps...>... — one base per model.
// Each base is independently a DomainArenaPopulator<TDomain> (NodeLink CRTP), so
// constructing ONE TypeList-specialized static instance self-registers N
// independent populator nodes, one per model.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

namespace crg::test::typelist_binding {

    struct FleetDomain {};

    struct Scout { u8 m_Data[8]; };
    struct Drone { u8 m_Data[8]; };

    struct IMoveTL {
        struct Params { float m_Speed{ 0.f }; };
    };

    template<typename TModel>
    struct DefaultMoveTL : public Capability<IMoveTL> {
        static void Execute(IMoveTL::Params& p) { p.m_Speed = 1.0f; }
    };

    using Robots = TypeList<Scout, Drone>;

} // namespace crg::test::typelist_binding

CRG_DECLARE_DOMAIN(crg::test::typelist_binding::FleetDomain)
CRG_DEFINE_DOMAIN(crg::test::typelist_binding::FleetDomain)

CRG_DECLARE_DOMAIN_MODELS(crg::test::typelist_binding::FleetDomain,
    crg::test::typelist_binding::Scout,
    crg::test::typelist_binding::Drone)

namespace {
    using namespace crg::test::typelist_binding;

    // ─── One declaration, one TypeList — fans DefaultMoveTL across both models ───
    static const CapabilityBinding<FleetDomain, Robots, DefaultMoveTL> s_RobotsBinding;
}

TEST_CASE("TypeList binding — a single TypeList<> declaration binds every listed model", "[typelist_binding]") {
    using namespace crg::test::typelist_binding;

    auto scoutToken = ModelToken<FleetDomain>::FromType<Scout>();
    auto droneToken = ModelToken<FleetDomain>::FromType<Drone>();

    auto scoutGate = CapabilityRouter<FleetDomain>::Find<IMoveTL>(scoutToken);
    auto droneGate = CapabilityRouter<FleetDomain>::Find<IMoveTL>(droneToken);

    REQUIRE(scoutGate);
    REQUIRE(droneGate);

    IMoveTL::Params scoutParams, droneParams;
    scoutGate(scoutParams);
    droneGate(droneParams);

    REQUIRE(scoutParams.m_Speed == 1.0f);
    REQUIRE(droneParams.m_Speed == 1.0f);
}

TEST_CASE("TypeList binding — each fanned-out model is an independent arena row", "[typelist_binding]") {
    using namespace crg::test::typelist_binding;

    // Both models resolve to distinct, valid DenseIDs -- the TypeList expansion
    // registered two separate rows, not one shared row.
    auto scoutToken = ModelToken<FleetDomain>::FromType<Scout>();
    auto droneToken = ModelToken<FleetDomain>::FromType<Drone>();

    REQUIRE(scoutToken.IsValid());
    REQUIRE(droneToken.IsValid());
    REQUIRE(!(scoutToken == droneToken));
}
