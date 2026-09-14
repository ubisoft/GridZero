// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// STAGE 07 — ModelShell: Brain Interface + Topological Routing
// ─────────────────────────────────────────────────────────────────────────────
// PROBLEM:
//   You have a polymorphic OOP contract with multiple virtual methods.
//   You also need contextual dispatch (the same model behaves differently at
//   Low vs High quality). How do you combine the Brain path (virtual interface)
//   with topological routing without giving up type safety?
//
// SOLUTION — ModelShell + Invoke / TryInvoke:
//   ModelShell<TDomain> is a 64-byte fixed-size buffer wrapper that erases the
//   concrete model type while keeping a dense ID for router lookups. There is
//   no heap fallback: an oversized model fails to compile, it never spills to
//   the heap.
//
//   Interface methods take (const ModelShell<TDomain>&, <context args>...) const.
//   - The shell is passed so the implementation can retrieve the original model
//     data via shell.Cast<TModel>() when needed.
//   - The context args serve double duty: they are forwarded to
//     CapabilityRouter::Find for tensor offset computation (topology) AND to
//     the virtual method as call arguments.
//
//   shell.Invoke<&IFoo::Method>(args...)
//     Hard-asserts that a binding exists. Prefer when the contract is mandatory.
//
//   shell.TryInvoke<&IFoo::Method>(args...)
//     Returns std::optional<R> (or void). Returns {} when the shell is empty
//     or when no binding was registered for the stored model type.
//
// WHAT'S NEW vs Stage 05:
//   - CapabilitySpace<Quality07> adds a 1D tensor over a Quality axis.
//   - ModelShell erases the concrete model type at the call site.
//   - Invoke/TryInvoke hide all routing machinery behind a single template call.
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
struct S07Domain {};
CRG_DECLARE_DOMAIN(S07Domain)
CRG_DEFINE_DOMAIN(S07Domain)

struct S07Unit  { crg::u8 m_Data[8]; };
struct S07Ghost {};        // declared but intentionally has no CapabilityBinding
CRG_DECLARE_DOMAIN_MODELS(S07Domain,
    S07Unit,
    S07Ghost)

// ─── Topology axis ────────────────────────────────────────────────────────────
//   Two quality tiers. The tensor has one row per registered model, two columns.
enum class Quality07 { Low, High };

namespace crg {
    template<> struct EnumTraits<Quality07> { static constexpr std::size_t Count = 2; };
}

// ─── Brain contract ───────────────────────────────────────────────────────────
//   Virtual methods → std::is_polymorphic_v<IRenderer07> == true
//                   → IsStaticContract<IRenderer07>         == false
//
//   Each method takes (const ModelShell<S07Domain>&, Quality07) const.
//   The shell is available for data access; Quality07 is the routed dimension.
struct IRenderer07 {
    virtual std::string_view Describe(const ModelShell<S07Domain>& shell, Quality07 q) const = 0;
    virtual ~IRenderer07() = default;
};

// ─── Routing traits: 1D tensor on Quality07 ───────────────────────────────────
namespace crg::routing {
    template<> struct CapabilityRoutingTraits<S07Domain, IRenderer07> {
        using SpaceType = CapabilitySpace<Quality07>;
    };
}

// ─── Brain capability: primary template (Low quality default) ─────────────────
//   Inherits the virtual interface directly — no Capability<> wrapper needed
//   for the Brain path. FillArena uses std::is_base_of_v<IRenderer07, Impl>.
template<typename TModel, typename TAt>
struct UnitRenderer : Capability<IRenderer07> {
    std::string_view Describe(const ModelShell<S07Domain>&, Quality07) const override {
        return "unit-low";
    }
};

// ─── Partial specialization: High quality cell ────────────────────────────────
//   At<Quality07::High> is the compile-time coordinate for column index 1.
//   Only this corner of the tensor gets the "high" behavior.
template<typename TModel>
struct UnitRenderer<TModel, At<Quality07::High>> : Capability<IRenderer07> {
    std::string_view Describe(const ModelShell<S07Domain>&, Quality07) const override {
        return "unit-high";
    }
};

namespace {
    static const CapabilityBinding<S07Domain, S07Unit, UnitRenderer> s_b;
    // S07Ghost deliberately has no binding here.
}

// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Stage 07 — IRenderer07 is a Brain contract (not DOD)", "[stage07][shell]") {
    // compile-time: virtual methods → Brain path → no Execute(Params&) needed
    STATIC_REQUIRE(capabilities::IsStaticContract<IRenderer07> == false);
}

TEST_CASE("Stage 07 — Invoke routes through topology and dispatches via virtual interface", "[stage07][shell]") {
    S07Unit myUnit{};
    ModelShell<S07Domain> shell(myUnit);

    // Invoke<&IFoo::Method>(args...) forwards args to both Find (topology offset)
    // and the virtual method call. Asserts on missing binding.
    auto low  = shell.Invoke<&IRenderer07::Describe>(Quality07::Low);
    auto high = shell.Invoke<&IRenderer07::Describe>(Quality07::High);

    REQUIRE(low  == "unit-low");
    REQUIRE(high == "unit-high");
}

TEST_CASE("Stage 07 — TryInvoke returns value when binding exists", "[stage07][shell]") {
    S07Unit myUnit{};
    ModelShell<S07Domain> shell(myUnit);

    // Returns std::optional<std::string_view>
    auto result = shell.TryInvoke<&IRenderer07::Describe>(Quality07::High);

    REQUIRE(result.has_value());
    REQUIRE(*result == "unit-high");
}

TEST_CASE("Stage 07 — TryInvoke returns empty optional for unbound model", "[stage07][shell]") {
    // S07Ghost is a registered model type (CRG_DECLARE_MODEL) but has no
    // CapabilityBinding<S07Domain, S07Ghost, ...> → Find returns nullptr.
    S07Ghost ghost{};
    ModelShell<S07Domain> shell(ghost);

    auto result = shell.TryInvoke<&IRenderer07::Describe>(Quality07::Low);

    // The wrapper evaluates to false — no binding was found for S07Ghost.
    REQUIRE(!result);
}

TEST_CASE("Stage 07 — ModelShell construction and type identity", "[stage07][shell]") {
    S07Unit myUnit{};
    ModelShell<S07Domain> shell(myUnit);

    // GetTypeKey returns the FNV-1a hash of "S07Unit" — stable across TUs.
    const crg::u64 expectedKey = crg::models::DomainTraits<S07Domain>::template TypeKey<S07Unit>;
    REQUIRE(shell.GetTypeKey() == expectedKey);
    REQUIRE(shell.GetTypeKey() != 0);
}
