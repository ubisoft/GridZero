// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// STAGE 02 — Brain Capability: OOP / Polymorphic Path
// ─────────────────────────────────────────────────────────────────────────────
// PROBLEM:
//   Some contracts are naturally polymorphic: multiple named virtual methods,
//   object semantics, familiar C++ OOP patterns. Forcing them into a single
//   Execute(Params&) function is awkward and loses the interface clarity.
//
// SOLUTION — the Brain path:
//   Contract = a struct (or class) with virtual methods — no Params member.
//   IsStaticContract<T> == false when T is polymorphic (std::is_polymorphic_v).
//   CapabilityHandle<TContract, false> stores a const TContract* and exposes
//   operator->() for natural "gate->Method()" call syntax.
//
//   The capability template inherits the virtual interface directly and overrides.
//   The router stores a pointer to the statically-allocated capability instance.
//   Cost: one pointer dereference + one virtual call. No heap allocation.
//
//   CapabilityBinding<TDomain, TModel, TCapabilities...>'s TModel slot accepts
//   either a single model or a crg::TypeList<TModels...> — the latter fans the
//   same capability set across every listed model from one declaration. The
//   matrix (models × capabilities) builds itself; see the last two cases below.
//
// WHEN TO USE:
//   - Contracts with multiple named methods (render, serialize, debug, …)
//   - OOP-style domains where virtual is the natural fit
//   - The call is not on the extreme hot path (< 1ns budget)
//
// WHAT'S NEW vs Stage 01:
//   First capability contract and binding. Virtual contract → IsStaticContract
//   == false → CapabilityHandle gains operator->() instead of operator().
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

// ─── Domain + Model ───────────────────────────────────────────────────────────
struct S02Domain {};
CRG_DECLARE_DOMAIN(S02Domain)
CRG_DEFINE_DOMAIN(S02Domain)

struct S02Unit { u8 m_Data[8]; };
CRG_DECLARE_DOMAIN_MODELS(S02Domain,
    S02Unit)

// ─── Brain contract: virtual methods, no Params ───────────────────────────────
//   std::is_polymorphic_v<ILogger02> == true
//   → IsStaticContract<ILogger02>       == false
//   → CapabilityHandle stores const ILogger02* and has operator->()
struct ILogger02 {
    virtual void Log(const char* msg) const = 0;
    virtual int  Level()             const = 0;
    virtual ~ILogger02() = default;
};

// ─── Brain capability: inherits the virtual interface directly ────────────────
//   No Capability<> wrapper needed — the SFINAE check in FillArena accepts
//   any type that is std::is_base_of_v<ILogger02, Impl>.
template<typename TModel>
struct ConsoleLogger02 : Capability<ILogger02> {
    void Log(const char* msg) const override { (void)msg; }
    int  Level()             const override { return 1; }
};

namespace {
    static const CapabilityBinding<S02Domain, S02Unit, ConsoleLogger02> s_b;
}

// ─── Model-set binding: TModel = a single model, or crg::TypeList<TModels...> ─
//   Same capability, same one-liner — fanned across every model in the list.
struct S02Scout { u8 m_Data[8]; };
struct S02Drone { u8 m_Data[8]; };
CRG_DECLARE_DOMAIN_MODELS(S02Domain,
    S02Scout,
    S02Drone)

using S02Squad = TypeList<S02Scout, S02Drone>;

namespace {
    static const CapabilityBinding<S02Domain, S02Squad, ConsoleLogger02> s_SquadBinding;
}

// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Stage 02 — Brain token is valid after Find", "[stage02][brain]") {
    auto token = ModelToken<S02Domain>::FromType<S02Unit>();
    auto gate  = CapabilityRouter<S02Domain>::Find<ILogger02>(token);

    REQUIRE(gate);
}

TEST_CASE("Stage 02 — operator->() dispatches through the virtual interface", "[stage02][brain]") {
    auto token = ModelToken<S02Domain>::FromType<S02Unit>();
    auto gate  = CapabilityRouter<S02Domain>::Find<ILogger02>(token);

    // gate is CapabilityHandle<ILogger02, false>.
    // operator->() returns const ILogger02* → virtual call resolves to ConsoleLogger02.
    REQUIRE(gate->Level() == 1);
    gate->Log("hello from stage 02"); // no crash = success
}

TEST_CASE("Stage 02 — Brain and Muscle path selected at compile time", "[stage02][brain]") {
    // The choice between Brain and Muscle is a zero-cost compile-time decision.
    // No runtime flag, no branch, no cost for the path you don't take.
    STATIC_REQUIRE(capabilities::IsStaticContract<ILogger02> == false);

    // For reference: a DOD contract (has Params, not polymorphic) would be true.
    struct IMover { struct Params { float m_Speed; }; };
    STATIC_REQUIRE(capabilities::IsStaticContract<IMover> == true);
}

TEST_CASE("Stage 02 — Monolithic build: Brain CapabilityHandle carries zero epoch overhead", "[stage02][brain]") {
    STATIC_REQUIRE(sizeof(CapabilityHandle<ILogger02>) == sizeof(void*));
}

TEST_CASE("Stage 02 — TModel accepts a single model or a TypeList<...> of models", "[stage02][brain][typelist]") {
    // One CapabilityBinding<S02Domain, S02Squad, ConsoleLogger02> declaration
    // above bound BOTH S02Scout and S02Drone — no per-model repetition.
    auto scoutToken = ModelToken<S02Domain>::FromType<S02Scout>();
    auto droneToken = ModelToken<S02Domain>::FromType<S02Drone>();

    auto scoutGate = CapabilityRouter<S02Domain>::Find<ILogger02>(scoutToken);
    auto droneGate = CapabilityRouter<S02Domain>::Find<ILogger02>(droneToken);

    REQUIRE(scoutGate);
    REQUIRE(droneGate);
    REQUIRE(scoutGate->Level() == 1);
    REQUIRE(droneGate->Level() == 1);
}
