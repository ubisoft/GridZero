// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// STAGE 02 — Muscle Capability: DOD Path, 0-Dimensional Routing
// ─────────────────────────────────────────────────────────────────────────────
// PROBLEM:
//   You want to bind a stateless function to a model type and invoke it via
//   its token — zero heap allocation, no virtual call on the hot path.
//
// SOLUTION — the Muscle path (Data-Oriented Design):
//   Contract    = a struct with a nested Params struct (no virtual methods).
//   Capability  = template<TModel, TAt> with a static Execute(Params&) function.
//   Binding     = a static CapabilityBinding in an anonymous namespace.
//   Find        = returns CapabilityHandle<TContract>. Invoke with operator().
//
//   CapabilitySpace<> — no axes → 0D tensor (Volume = 1).
//   Find() needs no context arguments: pure model dispatch.
//   The function pointer is stored directly in the CapabilityHandle.
//   One array lookup (dense ID row × volume offset) + one indirect call.
//
// WHEN TO USE:
//   - Per-type stateless logic with no contextual variation
//   - Hot-path budget: ~1 ns per call
//
// WHAT'S NEW vs Stage 01:
//   Contract + Capability template + CapabilityBinding + CapabilityRouter.
//   The routing tensor exists but has exactly one cell per registered model.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

#include <optional>
#include <type_traits>

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

// ─── Contract: a plain struct with Params ─────────────────────────────────────
//   No virtual methods → IsStaticContract<IMove02> == true at compile time.
//   The router will produce a CapabilityHandle with operator() for invocation.
struct IMove02 {
    struct Params {
        float m_Speed{ 0.f };
    };
};

// ─── Capability: template<TModel, TAt>, must provide static Execute ───────────
//   TModel = entity type (S02Unit here). TAt = tensor coordinate (At<> = 0D).
//   The Capability<IMove02> base is required for the SFINAE check in FillArena.
template<typename TModel>
struct DefaultMove02 : public Capability<IMove02> {
    static void Execute(IMove02::Params& p) {
        p.m_Speed = 1.0f;
    }
};

// ─── Binding: one static instance in anonymous namespace ─────────────────────
//   This single line is the entire registration. No Init(), no factory call.
//   The constructor links this into DomainArenaPopulator<S02Domain>.
//   CapabilityRouter::Find triggers Populate() on first call (lazy build).
namespace {
    static const CapabilityBinding<S02Domain, S02Unit, DefaultMove02> s_MoveBinding;
}

// ─── Contract with a custom Result: opt-in via a nested `Result` type ─────────
//   Result defaults to void (see IMove02 above); a contract that declares its
//   own Result makes operator()/TryInvoke return that type instead.
struct IHealthQuery02 {
    struct Params {
        std::uint32_t m_Id{ 0 };
    };
    using Result = int;
};

template<typename TModel>
struct DefaultHealthQuery02 : public Capability<IHealthQuery02> {
    static int Execute(IHealthQuery02::Params& p) {
        return static_cast<int>(p.m_Id) * 2;
    }
};

namespace {
    static const CapabilityBinding<S02Domain, S02Unit, DefaultHealthQuery02> s_HealthQueryBinding;
}

// ─── Contract whose Result is already std::optional<T> ───────────────────────
//   TryInvoke must return this type as-is, never std::optional<std::optional<T>>.
struct IOptionalQuery02 {
    struct Params {
        float m_Value{ 0.f };
    };
    using Result = std::optional<float>;
};

template<typename TModel>
struct DefaultOptionalQuery02 : public Capability<IOptionalQuery02> {
    static std::optional<float> Execute(IOptionalQuery02::Params& p) {
        if (p.m_Value < 0.f) return std::nullopt;
        return p.m_Value * 2.0f;
    }
};

namespace {
    static const CapabilityBinding<S02Domain, S02Unit, DefaultOptionalQuery02> s_OptionalQueryBinding;
}

// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Stage 02 — Find resolves and CapabilityHandle is valid", "[stage02][muscle]") {
    auto token = ModelToken<S02Domain>::FromType<S02Unit>();
    auto gate  = CapabilityRouter<S02Domain>::Find<IMove02>(token);

    REQUIRE(gate);
}

TEST_CASE("Stage 02 — DOD dispatch: operator() calls Execute in-place", "[stage02][muscle]") {
    auto token = ModelToken<S02Domain>::FromType<S02Unit>();
    auto gate  = CapabilityRouter<S02Domain>::Find<IMove02>(token);

    IMove02::Params p;
    gate(p); // zero heap allocation; one function-pointer call

    REQUIRE(p.m_Speed == 1.0f);
}

TEST_CASE("Stage 02 — Invalid token returns a null CapabilityHandle", "[stage02][muscle]") {
    // A default-constructed token has no slot → Find returns operator bool() == false.
    ModelToken<S02Domain> invalid{};
    auto gate = CapabilityRouter<S02Domain>::Find<IMove02>(invalid);

    REQUIRE(!gate);
}

TEST_CASE("Stage 02 — Monolithic build: CapabilityHandle carries zero epoch overhead", "[stage02][muscle]") {
    static_assert(sizeof(CapabilityHandle<IMove02>) == sizeof(void*));
    REQUIRE(sizeof(CapabilityHandle<IMove02>) == sizeof(void*));
}

TEST_CASE("Stage 02 — custom Result: operator() returns the Execute return value", "[stage02][muscle][result]") {
    auto token = ModelToken<S02Domain>::FromType<S02Unit>();
    auto gate  = CapabilityRouter<S02Domain>::Find<IHealthQuery02>(token);

    IHealthQuery02::Params p{ 21 };
    int result = gate(p);

    REQUIRE(result == 42);
}

TEST_CASE("Stage 02 — custom Result: TryInvoke wraps a bound call in std::optional", "[stage02][muscle][result]") {
    auto token = ModelToken<S02Domain>::FromType<S02Unit>();
    auto gate  = CapabilityRouter<S02Domain>::Find<IHealthQuery02>(token);

    static_assert(std::is_same_v<decltype(gate.TryInvoke(std::declval<IHealthQuery02::Params&>())), std::optional<int>>,
        "TryInvoke on a non-optional Result must return std::optional<Result>");

    IHealthQuery02::Params p{ 10 };
    std::optional<int> result = gate.TryInvoke(p);

    REQUIRE(result.has_value());
    REQUIRE(*result == 20);
}

TEST_CASE("Stage 02 — custom Result: TryInvoke returns an empty optional for an unbound handle", "[stage02][muscle][result]") {
    ModelToken<S02Domain> invalid{};
    auto gate = CapabilityRouter<S02Domain>::Find<IHealthQuery02>(invalid);
    REQUIRE(!gate);

    IHealthQuery02::Params p{ 5 };
    std::optional<int> result = gate.TryInvoke(p);

    REQUIRE(!result.has_value());
}

TEST_CASE("Stage 02 — Result already std::optional<T> is not double-wrapped by TryInvoke", "[stage02][muscle][result]") {
    auto token = ModelToken<S02Domain>::FromType<S02Unit>();
    auto gate  = CapabilityRouter<S02Domain>::Find<IOptionalQuery02>(token);

    static_assert(std::is_same_v<decltype(gate.TryInvoke(std::declval<IOptionalQuery02::Params&>())), std::optional<float>>,
        "TryInvoke must not wrap a Result that is already std::optional<T> a second time");

    IOptionalQuery02::Params positive{ 3.0f };
    std::optional<float> found = gate.TryInvoke(positive);
    REQUIRE(found.has_value());
    REQUIRE(*found == 6.0f);

    // Execute itself chose nullopt here -- with no second optional layer, this is
    // indistinguishable from "unbound" below. That collapse is the intended
    // trade-off of not double-wrapping an already-optional Result.
    IOptionalQuery02::Params negative{ -1.0f };
    std::optional<float> boundButEmpty = gate.TryInvoke(negative);
    REQUIRE(!boundButEmpty.has_value());

    ModelToken<S02Domain> invalid{};
    auto unboundGate = CapabilityRouter<S02Domain>::Find<IOptionalQuery02>(invalid);
    REQUIRE(!unboundGate);
    std::optional<float> unbound = unboundGate.TryInvoke(positive);
    REQUIRE(!unbound.has_value());
}
