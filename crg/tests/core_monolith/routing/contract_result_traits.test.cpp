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

    struct IHealthQuery {
        struct Params {
            std::uint32_t m_Id{ 0 };
            int m_Out{};
        };
    };

    template<typename TModel>
    struct DefaultHealthQuery : public Capability<IHealthQuery> {
        static void Execute(IHealthQuery::Params& p) {
            p.m_Out = static_cast<int>(p.m_Id) * 2;
        }
    };

    // ─── Contract whose Result is already std::optional<T> ───────────────────
    struct IOptionalQuery {
        struct Params {
            float m_Value{ 0.f };
            std::optional<float> m_Out{};
        };
    };

    template<typename TModel>
    struct DefaultOptionalQuery : public Capability<IOptionalQuery> {
        static void Execute(IOptionalQuery::Params& p) {
            if (p.m_Value >= 0.f) p.m_Out.emplace(p.m_Value * 2.0f);
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

TEST_CASE("Contract — Params with Out custom Result: POD", "[contract_result_traits][result]") {
    using namespace crg::test::contract_result_traits;
    auto token = ModelToken<ResultTraitsDomain>::FromType<ResultTraitsUnit>();
    auto gate  = CapabilityRouter<ResultTraitsDomain>::Find<IHealthQuery>(token);

    IHealthQuery::Params p{ 10 };
    gate.TryInvoke(p);

    REQUIRE(p.m_Out == 20);
}

TEST_CASE("Contract — Params with Out custom Result: std::optional", "[contract_result_traits][result]") {
    using namespace crg::test::contract_result_traits;
    ModelToken<ResultTraitsDomain> invalid{};
    auto gate = CapabilityRouter<ResultTraitsDomain>::Find<IOptionalQuery>(invalid);
    REQUIRE(!gate);

    IOptionalQuery::Params p{ 5.0f };
    gate.TryInvoke(p);

    REQUIRE(!p.m_Out.has_value());
}

