// Copyright (c) Ubisoft. All Rights Reserved.
// Licensed under the Apache 2.0 License. See LICENSE.md in the project root for license information.

// ═════════════════════════════════════════════════════════════════════════════
// CONTRACT RESULT TRAITS — custom Result opt-in, TryInvoke wrapping semantics
// ─────────────────────────────────────────────────────────────────────────────
// Execute defaults to returning void. A contract opts in to a non-void return
// by declaring a nested Result type (StaticResultOf<TContract>, defaults to
// void when absent). operator() and TryInvoke on BindingTarget/CapabilityHandle
// resolve to that Result. TryInvoke wraps it the same way ModelShell::TryInvoke
// does (crg::TryInvokeResult_t<Result>): void stays void; a non-optional Result
// becomes std::optional<Result>; a Result that is already std::optional<T> is
// returned as-is, never double-wrapped. See capability_binding.md, "Custom
// return values".
// ═════════════════════════════════════════════════════════════════════════════

#include "catch.hpp"
#include "crg/crg.hpp"

#include <optional>
#include <type_traits>

using namespace crg;
using namespace crg::models;
using namespace crg::routing;
using namespace crg::capabilities;

namespace crg::test::contract_result_traits {

    struct ResultTraitsDomain {};

    struct ResultTraitsUnit { u8 m_Data[8]; };

    // ─── Contract with a custom Result: opt-in via a nested `Result` type ────
    struct IHealthQuery {
        struct Params { std::uint32_t m_Id{ 0 }; };
        using Result = int;
    };

    template<typename TModel>
    struct DefaultHealthQuery : public Capability<IHealthQuery> {
        static int Execute(IHealthQuery::Params& p) {
            return static_cast<int>(p.m_Id) * 2;
        }
    };

    // ─── Contract whose Result is already std::optional<T> ───────────────────
    struct IOptionalQuery {
        struct Params { float m_Value{ 0.f }; };
        using Result = std::optional<float>;
    };

    template<typename TModel>
    struct DefaultOptionalQuery : public Capability<IOptionalQuery> {
        static std::optional<float> Execute(IOptionalQuery::Params& p) {
            if (p.m_Value < 0.f) return std::nullopt;
            return p.m_Value * 2.0f;
        }
    };

} // namespace crg::test::contract_result_traits

CRG_DECLARE_DOMAIN(crg::test::contract_result_traits::ResultTraitsDomain)
CRG_DEFINE_DOMAIN(crg::test::contract_result_traits::ResultTraitsDomain)

CRG_DECLARE_DOMAIN_MODELS(crg::test::contract_result_traits::ResultTraitsDomain,
    crg::test::contract_result_traits::ResultTraitsUnit)

namespace {
    using namespace crg::test::contract_result_traits;

    static const CapabilityBinding<ResultTraitsDomain, ResultTraitsUnit, DefaultHealthQuery> s_HealthQueryBinding;
    static const CapabilityBinding<ResultTraitsDomain, ResultTraitsUnit, DefaultOptionalQuery> s_OptionalQueryBinding;
}

TEST_CASE("Contract result traits — custom Result: operator() returns the Execute return value", "[contract_result_traits][result]") {
    using namespace crg::test::contract_result_traits;
    auto token = ModelToken<ResultTraitsDomain>::FromType<ResultTraitsUnit>();
    auto gate  = CapabilityRouter<ResultTraitsDomain>::Find<IHealthQuery>(token);

    IHealthQuery::Params p{ 21 };
    int result = gate(p);

    REQUIRE(result == 42);
}

TEST_CASE("Contract result traits — custom Result: TryInvoke wraps a bound call in std::optional", "[contract_result_traits][result]") {
    using namespace crg::test::contract_result_traits;
    auto token = ModelToken<ResultTraitsDomain>::FromType<ResultTraitsUnit>();
    auto gate  = CapabilityRouter<ResultTraitsDomain>::Find<IHealthQuery>(token);

    static_assert(std::is_same_v<decltype(gate.TryInvoke(std::declval<IHealthQuery::Params&>())), std::optional<int>>,
        "TryInvoke on a non-optional Result must return std::optional<Result>");

    IHealthQuery::Params p{ 10 };
    std::optional<int> result = gate.TryInvoke(p);

    REQUIRE(result.has_value());
    REQUIRE(*result == 20);
}

TEST_CASE("Contract result traits — custom Result: TryInvoke returns an empty optional for an unbound handle", "[contract_result_traits][result]") {
    using namespace crg::test::contract_result_traits;
    ModelToken<ResultTraitsDomain> invalid{};
    auto gate = CapabilityRouter<ResultTraitsDomain>::Find<IHealthQuery>(invalid);
    REQUIRE(!gate);

    IHealthQuery::Params p{ 5 };
    std::optional<int> result = gate.TryInvoke(p);

    REQUIRE(!result.has_value());
}

TEST_CASE("Contract result traits — Result already std::optional<T> is not double-wrapped by TryInvoke", "[contract_result_traits][result]") {
    using namespace crg::test::contract_result_traits;
    auto token = ModelToken<ResultTraitsDomain>::FromType<ResultTraitsUnit>();
    auto gate  = CapabilityRouter<ResultTraitsDomain>::Find<IOptionalQuery>(token);

    static_assert(std::is_same_v<decltype(gate.TryInvoke(std::declval<IOptionalQuery::Params&>())), std::optional<float>>,
        "TryInvoke must not wrap a Result that is already std::optional<T> a second time");

    IOptionalQuery::Params positive{ 3.0f };
    std::optional<float> found = gate.TryInvoke(positive);
    REQUIRE(found.has_value());
    REQUIRE(*found == 6.0f);

    // Execute itself chose nullopt here -- with no second optional layer, this is
    // indistinguishable from "unbound" below. That collapse is the intended
    // trade-off of not double-wrapping an already-optional Result.
    IOptionalQuery::Params negative{ -1.0f };
    std::optional<float> boundButEmpty = gate.TryInvoke(negative);
    REQUIRE(!boundButEmpty.has_value());

    ModelToken<ResultTraitsDomain> invalid{};
    auto unboundGate = CapabilityRouter<ResultTraitsDomain>::Find<IOptionalQuery>(invalid);
    REQUIRE(!unboundGate);
    std::optional<float> unbound = unboundGate.TryInvoke(positive);
    REQUIRE(!unbound.has_value());
}
