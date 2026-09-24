// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// STAGE 06 — ModelShell: Brain Interface + Topological Routing
// ─────────────────────────────────────────────────────────────────────────────
// PROBLEM:
//   You have a polymorphic OOP contract with multiple virtual methods, and
//   contextual dispatch (the same model behaves differently at Low vs High
//   quality). Because the contract is type-erased, the capability implementing
//   it does not receive the model directly — how does it get at the model's
//   own data at all?
//
// SOLUTION — ModelShell::Cast<TModel>() inside the capability:
//   Every Brain method that participates in topological routing takes
//   (const ModelShell<TDomain>&, <context args>...) const. The shell carries
//   the erased model plus a DenseID for router lookups — the capability
//   recovers the concrete model with shell.Cast<TModel>() and reads its data
//   directly. That call, inside the capability body, is the actual point of
//   this stage; ModelShell<TDomain> is a fixed-buffer wrapper (64 bytes),
//   with no heap fallback: an oversized model fails to compile instead of
//   spilling to the heap.
//
//   Invoke<&IFoo::Method>(args...) / TryInvoke<&IFoo::Method>(args...) are
//   how the caller reaches that capability — the args do double duty (forwarded
//   to Find() for the topology offset, then to the method as call arguments).
//   That call-site mechanics is secondary here; the load-bearing fact is what
//   happens inside Describe().
//
// WHAT'S NEW vs Stage 05:
//   - CapabilitySpace<Quality06> adds a 1D tensor over a Quality axis.
//   - ModelShell erases the concrete model type at the call site.
//   - The capability itself calls shell.Cast<TModel>() to read model data —
//     the type erasure is undone exactly once, inside the implementation.
//   - TryInvoke demonstrates graceful fallback for an unbound model type.
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"
#include <string_view>

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;
using crg::shell::ModelShell;

// ─── Domain + Models ──────────────────────────────────────────────────────────
struct S06Domain {};
CRG_DECLARE_DOMAIN(S06Domain)
CRG_DEFINE_DOMAIN(S06Domain)

// S06Unit carries real data (m_Health), not just filler bytes — the whole
// point of this stage is a capability reading it back out through the shell.
struct S06Unit  { float m_Health{ 100.f }; };
struct S06Ghost {};        // declared but intentionally has no CapabilityBinding
CRG_DECLARE_DOMAIN_MODELS(S06Domain,
    S06Unit,
    S06Ghost)

// ─── Topology axis ────────────────────────────────────────────────────────────
//   Two quality tiers. The tensor has one row per registered model, two columns.
enum class Quality06 { Low, High };

namespace crg {
    template<> struct EnumTraits<Quality06> { static constexpr std::size_t Count = 2; };
}

// ─── Brain contract ───────────────────────────────────────────────────────────
//   Virtual methods → std::is_polymorphic_v<IRenderer06> == true
//                   → IsStaticContract<IRenderer06>         == false
//
//   Each method takes (const ModelShell<S06Domain>&, Quality06) const.
//   Quality06 is the routed dimension; the shell is how the implementation
//   gets back to the concrete S06Unit.
struct IRenderer06 {
    virtual std::string_view Describe(const ModelShell<S06Domain>& shell, Quality06 q) const = 0;
    virtual ~IRenderer06() = default;
};

// ─── Routing traits: 1D tensor on Quality06 ───────────────────────────────────
namespace crg::routing {
    template<> struct CapabilityRoutingTraits<S06Domain, IRenderer06> {
        using SpaceType = CapabilitySpace<Quality06>;
    };
}

// ─── Brain capability: primary template (Low quality default) ─────────────────
//   shell.Cast<TModel>() recovers the concrete model from the type-erased
//   shell, using the capability's OWN template parameter — not a hardcoded
//   type. That is what keeps this capability reusable if it is ever bound
//   across a TypeList of models (Stage 02): each instantiation casts back
//   to its own TModel, never to a name baked in at authoring time.
template<typename TModel, typename TAt>
struct UnitRenderer : Capability<IRenderer06> {
    std::string_view Describe(const ModelShell<S06Domain>& shell, Quality06) const override {
        const TModel& unit = shell.Cast<TModel>();
        return (unit.m_Health > 0.f) ? "unit-low-alive" : "unit-low-dead";
    }
};

// ─── Partial specialization: High quality cell ────────────────────────────────
//   At<Quality06::High> is the compile-time coordinate for column index 1.
//   Only this corner of the tensor gets the "high" behavior — but it still
//   goes through shell.Cast<TModel>() the same way.
template<typename TModel>
struct UnitRenderer<TModel, At<Quality06::High>> : Capability<IRenderer06> {
    std::string_view Describe(const ModelShell<S06Domain>& shell, Quality06) const override {
        const TModel& unit = shell.Cast<TModel>();
        return (unit.m_Health > 0.f) ? "unit-high-alive" : "unit-high-dead";
    }
};

namespace {
    static const CapabilityBinding<S06Domain, S06Unit, UnitRenderer> s_b;
    // S06Ghost deliberately has no binding here.
}

// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Stage 06 — IRenderer06 is a Brain contract (not DOD)", "[stage06][shell]") {
    // compile-time: virtual methods → Brain path → no Execute(Params&) needed
    STATIC_REQUIRE(capabilities::IsStaticContract<IRenderer06> == false);
}

TEST_CASE("Stage 06 — the capability reads model data via shell.Cast<TModel>()", "[stage06][shell]") {
    // Same quality tier, two different models: the output differs only
    // because Describe() calls shell.Cast<S06Unit>() and reads m_Health —
    // proof the cast is real, not a decorative parameter.
    S06Unit alive{ 100.f };
    S06Unit dead{ 0.f };
    ModelShell<S06Domain> aliveShell(alive);
    ModelShell<S06Domain> deadShell(dead);

    REQUIRE(aliveShell.Invoke<&IRenderer06::Describe>(Quality06::Low) == "unit-low-alive");
    REQUIRE(deadShell.Invoke<&IRenderer06::Describe>(Quality06::Low)  == "unit-low-dead");
}

TEST_CASE("Stage 06 — Invoke routes through topology and dispatches via virtual interface", "[stage06][shell]") {
    S06Unit myUnit{};
    ModelShell<S06Domain> shell(myUnit);

    // Invoke<&IFoo::Method>(args...) forwards args to both Find (topology offset)
    // and the virtual method call. Asserts on missing binding.
    auto low  = shell.Invoke<&IRenderer06::Describe>(Quality06::Low);
    auto high = shell.Invoke<&IRenderer06::Describe>(Quality06::High);

    REQUIRE(low  == "unit-low-alive");
    REQUIRE(high == "unit-high-alive");
}

TEST_CASE("Stage 06 — TryInvoke returns value when binding exists", "[stage06][shell]") {
    S06Unit myUnit{};
    ModelShell<S06Domain> shell(myUnit);

    // Returns std::optional<std::string_view>
    auto result = shell.TryInvoke<&IRenderer06::Describe>(Quality06::High);

    REQUIRE(result.has_value());
    REQUIRE(*result == "unit-high-alive");
}

TEST_CASE("Stage 06 — TryInvoke returns empty optional for unbound model", "[stage06][shell]") {
    // S06Ghost is a registered model type (CRG_DECLARE_MODEL) but has no
    // CapabilityBinding<S06Domain, S06Ghost, ...> → Find returns nullptr.
    S06Ghost ghost{};
    ModelShell<S06Domain> shell(ghost);

    auto result = shell.TryInvoke<&IRenderer06::Describe>(Quality06::Low);

    // The wrapper evaluates to false — no binding was found for S06Ghost.
    REQUIRE(!result);
}

TEST_CASE("Stage 06 — ModelShell construction and type identity", "[stage06][shell]") {
    S06Unit myUnit{};
    ModelShell<S06Domain> shell(myUnit);

    // GetTypeKey returns the FNV-1a hash of "S06Unit" — stable across TUs.
    const crg::u64 expectedKey = crg::models::DomainTraits<S06Domain>::template TypeKey<S06Unit>;
    REQUIRE(shell.GetTypeKey() == expectedKey);
    REQUIRE(shell.GetTypeKey() != 0);
}
