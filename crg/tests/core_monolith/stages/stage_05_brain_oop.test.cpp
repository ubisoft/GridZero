// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// STAGE 05 — Brain Capability: OOP / Polymorphic Path
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
// WHEN TO USE:
//   - Contracts with multiple named methods (render, serialize, debug, …)
//   - OOP-style domains where virtual is the natural fit
//   - The call is not on the extreme hot path (< 1ns budget)
//
// WHAT'S NEW vs Stages 02–04:
//   Virtual contract → IsStaticContract == false → CapabilityHandle gains
//   operator->() instead of operator(). Everything else is unchanged.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

// ─── Domain + Model ───────────────────────────────────────────────────────────
struct S05Domain {};
CRG_DECLARE_DOMAIN(S05Domain)
CRG_DEFINE_DOMAIN(S05Domain)

struct S05Unit { u8 m_Data[8]; };
CRG_DECLARE_DOMAIN_MODELS(S05Domain,
    S05Unit)

// ─── Brain contract: virtual methods, no Params ───────────────────────────────
//   std::is_polymorphic_v<ILogger05> == true
//   → IsStaticContract<ILogger05>       == false
//   → CapabilityHandle stores const ILogger05* and has operator->()
struct ILogger05 {
    virtual void Log(const char* msg) const = 0;
    virtual int  Level()             const = 0;
    virtual ~ILogger05() = default;
};

// ─── Brain capability: inherits the virtual interface directly ────────────────
//   No Capability<> wrapper needed — the SFINAE check in FillArena accepts
//   any type that is std::is_base_of_v<ILogger05, Impl>.
template<typename TModel>
struct ConsoleLogger05 : Capability<ILogger05> {
    void Log(const char* msg) const override { (void)msg; }
    int  Level()             const override { return 1; }
};

namespace {
    static const CapabilityBinding<S05Domain, S05Unit, ConsoleLogger05> s_b;
}

// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Stage 05 — Brain token is valid after Find", "[stage05][brain]") {
    auto token = ModelToken<S05Domain>::FromType<S05Unit>();
    auto gate  = CapabilityRouter<S05Domain>::Find<ILogger05>(token);

    REQUIRE(gate);
}

TEST_CASE("Stage 05 — operator->() dispatches through the virtual interface", "[stage05][brain]") {
    auto token = ModelToken<S05Domain>::FromType<S05Unit>();
    auto gate  = CapabilityRouter<S05Domain>::Find<ILogger05>(token);

    // gate is CapabilityHandle<ILogger05, false>.
    // operator->() returns const ILogger05* → virtual call resolves to ConsoleLogger05.
    REQUIRE(gate->Level() == 1);
    gate->Log("hello from stage 05"); // no crash = success
}

TEST_CASE("Stage 05 — Brain and Muscle path selected at compile time", "[stage05][brain]") {
    // The choice between Brain and Muscle is a zero-cost compile-time decision.
    // No runtime flag, no branch, no cost for the path you don't take.
    STATIC_REQUIRE(capabilities::IsStaticContract<ILogger05> == false);

    // For reference: a DOD contract (has Params, not polymorphic) would be true.
    struct IMover { struct Params { float m_Speed; }; };
    STATIC_REQUIRE(capabilities::IsStaticContract<IMover> == true);
}

TEST_CASE("Stage 05 — Monolithic build: Brain CapabilityHandle carries zero epoch overhead", "[stage05][brain]") {
    STATIC_REQUIRE(sizeof(CapabilityHandle<ILogger05>) == sizeof(void*));
}
