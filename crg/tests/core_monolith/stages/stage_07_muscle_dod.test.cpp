// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// STAGE 07 — Muscle Capability: DOD Path, unrouted
// ─────────────────────────────────────────────────────────────────────────────
// PROBLEM:
//   You want to bind a stateless function to a model type and invoke it via
//   its token, with an interface that is strictly single-entry-point.
//
// SOLUTION — the Muscle path (Data-Oriented Design):
//   Contract    = a struct with a nested Params struct (no virtual methods).
//   Capability  = template<TModel> with a static Execute(Params&) function.
//   Binding     = a static CapabilityBinding in an anonymous namespace.
//   Find        = returns CapabilityHandle<TContract>. Invoke with operator().
//
//   Unrouted (no axis) → CapImplSel binds TCap<TModel>, arity-1, no TAt at
//   all — TAt only exists once an axis is added (Stage 03).
//
//   This is NOT chosen for speed: dod_vs_oop.bench.cpp measures a tie against
//   the Brain/virtual path. The reason to choose Muscle is the shape of the
//   contract itself — it is strictly single-Execute (verified by
//   HasStaticExecute), unlike Brain's multi-method interfaces (Stage 02).
//   That single entry point reduces to one plain function pointer — a pure
//   C-ABI slot, not a C++ vtable slot — which is what makes the contract
//   fillable by anything that can produce a matching C function pointer.
//
// WHEN TO USE:
//   - The interface is naturally a single entry point, not several methods
//   - The binding needs to reduce to a plain C-ABI function pointer
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

// ─── Domain + Model ───────────────────────────────────────────────────────────
struct S07Domain {};
CRG_DECLARE_DOMAIN(S07Domain)
CRG_DEFINE_DOMAIN(S07Domain)

struct S07Unit { u8 m_Data[8]; };
CRG_DECLARE_DOMAIN_MODELS(S07Domain,
    S07Unit)

// ─── Contract: a plain struct with Params ─────────────────────────────────────
//   No virtual methods → IsStaticContract<IMove07> == true at compile time.
//   The router will produce a CapabilityHandle with operator() for invocation.
struct IMove07 {
    struct Params {
        float m_Speed{ 0.f };
    };
};

// ─── Capability: template<TModel>, must provide static Execute ────────────────
//   Unrouted → arity-1, no TAt. The Capability<IMove07> base is required for
//   the SFINAE check in FillArena.
template<typename TModel>
struct DefaultMove07 : public Capability<IMove07> {
    static void Execute(IMove07::Params& p) {
        p.m_Speed = 1.0f;
    }
};

// ─── Binding: one static instance in anonymous namespace ─────────────────────
//   This single line is the entire registration. No Init(), no factory call.
//   The constructor links this into DomainArenaPopulator<S07Domain>.
//   CapabilityRouter::Find triggers Populate() on first call (lazy build).
namespace {
    static const CapabilityBinding<S07Domain, S07Unit, DefaultMove07> s_MoveBinding;
}

// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Stage 07 — Find resolves and CapabilityHandle is valid", "[stage07][muscle]") {
    auto token = ModelToken<S07Domain>::FromType<S07Unit>();
    auto gate  = CapabilityRouter<S07Domain>::Find<IMove07>(token);

    REQUIRE(gate);
}

TEST_CASE("Stage 07 — DOD dispatch: operator() calls Execute in-place", "[stage07][muscle]") {
    auto token = ModelToken<S07Domain>::FromType<S07Unit>();
    auto gate  = CapabilityRouter<S07Domain>::Find<IMove07>(token);

    IMove07::Params p;
    gate(p); // zero heap allocation; one function-pointer call

    REQUIRE(p.m_Speed == 1.0f);
}

TEST_CASE("Stage 07 — Invalid token returns a null CapabilityHandle", "[stage07][muscle]") {
    // A default-constructed token has no slot → Find returns operator bool() == false.
    ModelToken<S07Domain> invalid{};
    auto gate = CapabilityRouter<S07Domain>::Find<IMove07>(invalid);

    REQUIRE(!gate);
}

TEST_CASE("Stage 07 — Monolithic build: CapabilityHandle carries zero epoch overhead", "[stage07][muscle]") {
    static_assert(sizeof(CapabilityHandle<IMove07>) == sizeof(void*));
    REQUIRE(sizeof(CapabilityHandle<IMove07>) == sizeof(void*));
}
